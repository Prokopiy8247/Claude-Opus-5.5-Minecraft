// Overworld terrain: climate -> height/scale -> 3D density -> caves -> surface rules -> fluids, then ores,
// features, cave decoration, structures and initial animals.
#include "Gen/MCOverworldGen.h"
#include "Gen/MCFeatures.h"
#include "World/MCBlockEntity.h"

using namespace MCNoiseUtil;

namespace
{
	constexpr int32 GridXY = 4;
	constexpr int32 GridZ = 8;
	constexpr int32 CaveGridZ = 4;
	constexpr int32 NZNodes = MC::WorldHeight / GridZ + 1;       // 49
	constexpr int32 NCaveZNodes = MC::WorldHeight / CaveGridZ + 1; // 97

	enum ETBand { TFrozen, TCold, TTemperate, TWarm, THot };
	enum EHBand { HArid, HDry, HNeutral, HWet, HHumid };

	FORCEINLINE int32 TempBand(float T) { return T < -0.45f ? TFrozen : T < -0.15f ? TCold : T < 0.2f ? TTemperate : T < 0.55f ? TWarm : THot; }
	FORCEINLINE int32 HumidBand(float H) { return H < -0.35f ? HArid : H < -0.1f ? HDry : H < 0.1f ? HNeutral : H < 0.3f ? HWet : HHumid; }
	FORCEINLINE uint8 B(EMCBiome Bi) { return (uint8)Bi; }
	FORCEINLINE int32 ProtoIdx(int32 X, int32 Y, int32 Z) { return X + Y * 16 + (Z - MC::MinZ) * 256; }

	const double ContX[] = { -1.25, -0.7, -0.42, -0.26, -0.17, -0.11, -0.05, 0.0, 0.12, 0.3, 0.5, 0.75, 1.15 };
	const double ContY[] = { 16.0, 25.0, 33.0, 43.0, 51.0, 57.5, 62.0, 64.5, 67.0, 71.0, 76.0, 84.0, 94.0 };
}

FMCOverworldGen::FMCOverworldGen(uint64 InSeed)
	: FMCWorldGenerator(InSeed, EMCDimension::Overworld)
{
	auto S = [this](uint64 Salt) { return MCHash::SplitMix64(Seed ^ (Salt * 0x9E3779B97F4A7C15ull)); };
	NContinental.Init(S(1), 5, 1.0 / 900.0, 0.5);
	NErosion.Init(S(2), 4, 1.0 / 700.0, 0.5);
	NWeird.Init(S(3), 3, 1.0 / 520.0, 0.5);
	NTemp.Init(S(4), 3, 1.0 / 820.0, 0.5);
	NHumid.Init(S(5), 3, 1.0 / 640.0, 0.5);
	NShiftX.Init(S(6), 2, 1.0 / 280.0, 0.5);
	NShiftY.Init(S(7), 2, 1.0 / 280.0, 0.5);
	NRidge.Init(S(8), 5, 1.0 / 420.0, 0.55);
	NDetail.Init(S(9), 4, 1.0 / 150.0, 0.5);
	NCheese.Init(S(10), 3, 1.0 / 72.0, 0.5);
	NSpag1.Init(S(11), 2, 1.0 / 60.0, 0.5);
	NSpag2.Init(S(12), 2, 1.0 / 60.0, 0.5);
	NSpagWidth.Init(S(13), 1, 1.0 / 180.0, 0.5);
	NNoodle1.Init(S(14), 1, 1.0 / 26.0, 0.5);
	NNoodle2.Init(S(15), 1, 1.0 / 26.0, 0.5);
	NAquifer.Init(S(16), 3, 1.0 / 90.0, 0.5);
	NCaveBiome1.Init(S(17), 2, 1.0 / 260.0, 0.5);
	NCaveBiome2.Init(S(18), 2, 1.0 / 240.0, 0.5);
	NSurface.Init(S(19), 3, 1.0 / 36.0, 0.5);
	NBand.Init(S(20), 2, 1.0 / 90.0, 0.5);
	NClay.Init(S(21), 2, 1.0 / 30.0, 0.5);
	NIceberg.Init(S(22), 2, 1.0 / 60.0, 0.5);
	NPillar.Init(S(23), 2, 1.0 / 20.0, 0.5);
	NVein.Init(S(24), 2, 1.0 / 60.0, 0.5);
	NVeinGap.Init(S(25), 1, 1.0 / 12.0, 0.5);
	NSulfurBand.Init(S(26), 2, 1.0 / 40.0, 0.5);
	NRiver.Init(S(27), 3, 1.0 / 520.0, 0.5);
	NEntrance.Init(S(28), 2, 1.0 / 90.0, 0.5);
	NMushroom.Init(S(29), 2, 1.0 / 200.0, 0.5);
	Protos = MakeShared<FProtoCache>();

	auto Set = [this](const TCHAR* Name, int32 Spacing, int32 Sep, uint32 Salt, int32 Radius, bool bTri = false)
	{
		FMCStructureSet St; St.Type = FName(Name); St.Spacing = Spacing; St.Separation = Sep; St.Salt = Salt; St.MaxRadiusChunks = Radius; St.bTriangular = bTri;
		StructureSets.Add(St);
	};
	Set(TEXT("village"), 26, 8, 10387312, 6);
	Set(TEXT("desert_pyramid"), 30, 8, 14357617, 2);
	Set(TEXT("jungle_temple"), 30, 8, 14357619, 2);
	Set(TEXT("swamp_hut"), 30, 8, 14357620, 1);
	Set(TEXT("igloo"), 30, 8, 14357618, 1);
	Set(TEXT("pillager_outpost"), 32, 8, 165745296, 2);
	Set(TEXT("ruined_portal"), 32, 12, 34222645, 1);
	Set(TEXT("shipwreck"), 24, 4, 165745295, 2);
	Set(TEXT("ocean_ruin"), 20, 8, 14357621, 2);
	Set(TEXT("ocean_monument"), 32, 5, 10387313, 4, true);
	Set(TEXT("woodland_mansion"), 60, 20, 10387319, 4, true);
	Set(TEXT("ancient_city"), 24, 8, 20083232, 5);
	Set(TEXT("trail_ruins"), 34, 8, 83469867, 2);
	Set(TEXT("trial_chambers"), 34, 12, 94251327, 4);
	Set(TEXT("buried_treasure"), 12, 4, 10387320, 1);
	Set(TEXT("mineshaft"), 12, 4, 87451231, 6);
	ComputeStrongholds();
}

// ---------------------------------------------------------------------------------------------------------------------
// Climate & height

FMCClimate FMCOverworldGen::SampleClimate(double X, double Y) const
{
	FMCClimate Cl;
	const double SX = X + NShiftX.Sample2(X, Y) * 55.0;
	const double SY = Y + NShiftY.Sample2(X, Y) * 55.0;
	Cl.C = (float)FMath::Clamp(NContinental.Sample2(SX, SY) * 1.7 + 0.12, -1.3, 1.3);
	Cl.E = (float)FMath::Clamp(NErosion.Sample2(SX, SY) * 1.8, -1.2, 1.2);
	Cl.W = (float)FMath::Clamp(NWeird.Sample2(SX, SY) * 1.8, -1.2, 1.2);
	Cl.T = (float)FMath::Clamp(NTemp.Sample2(SX, SY) * 1.7, -1.2, 1.2);
	Cl.H = (float)FMath::Clamp(NHumid.Sample2(SX, SY) * 1.7, -1.2, 1.2);
	Cl.PV = 1.f - FMath::Abs(3.f * FMath::Abs(Cl.W) - 2.f);

	double H = Spline(Cl.C, ContX, ContY, UE_ARRAY_COUNT(ContX));
	const double Inland = SmoothStep(-0.04, 0.35, Cl.C);
	const double Mountain = Inland * SmoothStep(-0.02, -0.62, Cl.E);
	const double Ridge = NRidge.Ridged2(SX, SY);
	const double Peaks = Mountain * (FMath::Pow(FMath::Clamp(Ridge, 0.0, 1.0), 1.9) * 175.0 + 22.0 * Mountain);
	const double HillMask = Inland * (1.0 - Mountain * 0.7) * SmoothStep(0.75, 0.0, Cl.E);
	const double Hills = HillMask * (NDetail.Sample2(X, Y) * 18.0 + FMath::Max(0.f, Cl.PV) * 9.0);
	H += Peaks + Hills;

	// mushroom islands rise from the deepest oceans
	if (Cl.C < -0.95)
	{
		const double M = SmoothStep(0.15, 0.45, NMushroom.Sample2(X, Y));
		H = FMath::Lerp(H, 67.0 + NDetail.Sample2(X, Y) * 4.0, M);
	}

	// rivers carve through lowlands
	const double RiverN = FMath::Abs(NRiver.Sample2(SX, SY));
	const double Riverness = (1.0 - SmoothStep(0.012, 0.05, RiverN)) * (1.0 - Mountain) * SmoothStep(-0.06, 0.08, Cl.C);
	if (Riverness > 0.0 && H > 57.5)
	{
		H = FMath::Lerp(H, 57.5, Riverness);
	}
	Cl.bRiver = Riverness > 0.55 && H < 61.0;
	Cl.BaseHeight = (float)H;
	Cl.Scale = (float)(1.6 + Mountain * 21.0 + Inland * FMath::Abs(Cl.W) * 2.5);
	Cl.Biome = PickSurfaceBiome(Cl, (int32)H);
	return Cl;
}

uint8 FMCOverworldGen::PickSurfaceBiome(const FMCClimate& Cl, int32 Z) const
{
	using E = EMCBiome;
	const int32 T = TempBand(Cl.T), H = HumidBand(Cl.H);
	// oceans
	if (Cl.C < -0.14f && Z < MC::SeaLevel - 1)
	{
		if (Cl.C < -0.95f && Z >= MC::SeaLevel - 1) return B(E::MushroomFields);
		const bool bDeep = Cl.C < -0.4f;
		if (T == TFrozen) return B(bDeep ? E::DeepFrozenOcean : E::FrozenOcean);
		if (T == TCold) return B(bDeep ? E::DeepColdOcean : E::ColdOcean);
		if (T == TTemperate) return B(bDeep ? E::DeepOcean : E::Ocean);
		if (T == TWarm) return B(bDeep ? E::DeepLukewarmOcean : E::LukewarmOcean);
		return B(E::WarmOcean);
	}
	if (Cl.C < -0.95f) return B(E::MushroomFields);
	if (Cl.bRiver) return B(T == TFrozen ? E::FrozenRiver : E::River);
	const float Mountain = FMath::Clamp((-0.02f - Cl.E) / 0.6f, 0.f, 1.f);
	// peaks and slopes
	if (Z > 175)
	{
		if (T <= TCold) return B(Cl.W > 0.f ? E::JaggedPeaks : E::FrozenPeaks);
		if (T == TTemperate) return B(Cl.W > 0.3f ? E::JaggedPeaks : E::StonyPeaks);
		return B(E::StonyPeaks);
	}
	if (Z > 130)
	{
		if (T <= TCold) return B(H >= HNeutral ? E::SnowySlopes : E::Grove);
		if (T == TTemperate) return B(Cl.W > 0.35f ? E::CherryGrove : (H >= HWet ? E::Grove : E::Meadow));
		if (T == TWarm) return B(E::Meadow);
		return B(E::WindsweptSavanna);
	}
	// beaches
	if (Cl.C < -0.02f && Z <= MC::SeaLevel + 3)
	{
		if (Mountain > 0.35f) return B(E::StonyShore);
		if (T == TFrozen) return B(E::SnowyBeach);
		if (T == THot && H <= HDry) return B(E::Desert);
		return B(E::Beach);
	}
	// swamps near sea level
	if (Z <= MC::SeaLevel + 3 && H >= HWet && Cl.C > -0.02f && Cl.C < 0.45f)
	{
		if (T == THot || (T == TWarm && H == HHumid)) return B(E::MangroveSwamp);
		if (T == TTemperate || T == TWarm) return B(E::Swamp);
	}
	// windswept hills
	if (Mountain > 0.3f && Z > 95 && T <= TTemperate)
	{
		if (H <= HArid) return B(E::WindsweptGravellyHills);
		if (H >= HWet) return B(E::WindsweptForest);
		return B(E::WindsweptHills);
	}
	if (Z > 100 && Z <= 130 && T == TTemperate && H <= HNeutral) return B(E::Meadow);
	switch (T)
	{
	case TFrozen:
		if (H <= HDry) return B(Cl.W > 0.55f ? E::IceSpikes : E::SnowyPlains);
		if (H == HNeutral) return B(E::SnowyPlains);
		return B(E::SnowyTaiga);
	case TCold:
		if (H <= HArid) return B(E::Plains);
		if (H == HDry) return B(E::Plains);
		if (H == HNeutral) return B(E::Forest);
		if (H == HWet) return B(Cl.W > 0.4f ? E::OldGrowthPineTaiga : E::Taiga);
		return B(Cl.W > 0.2f ? E::OldGrowthSpruceTaiga : E::Taiga);
	case TTemperate:
		if (H <= HArid) return B(Cl.W > 0.45f ? E::FlowerForest : E::Plains);
		if (H == HDry) return B(Cl.W > 0.5f ? E::SunflowerPlains : E::Plains);
		if (H == HNeutral) return B(Cl.W < -0.45f ? E::FlowerForest : E::Forest);
		if (H == HWet) return B(Cl.W > 0.45f ? E::OldGrowthBirchForest : E::BirchForest);
		return B(Cl.W < -0.5f ? E::PaleGarden : E::DarkForest);
	case TWarm:
		if (H <= HArid) return B(Z > 88 ? E::SavannaPlateau : E::Savanna);
		if (H == HDry) return B(E::Savanna);
		if (H == HNeutral) return B(Cl.W > 0.3f ? E::Plains : E::Forest);
		if (H == HWet) return B(E::SparseJungle);
		return B(Cl.W > 0.3f ? E::BambooJungle : E::Jungle);
	default: // hot
		if (H <= HDry) return B(E::Desert);
		if (H == HNeutral) return B(Cl.E > 0.35f ? (Cl.W > 0.3f ? E::ErodedBadlands : E::Badlands) : E::Savanna);
		if (H == HWet) return B(Z > 90 ? E::WoodedBadlands : E::Badlands);
		return B(Cl.W > 0.2f ? E::BambooJungle : E::Jungle);
	}
}

uint8 FMCOverworldGen::PickCaveBiome(int32 X, int32 Y, int32 Z, const FMCClimate& Cl) const
{
	const double A = NCaveBiome1.Sample3(X, Y, Z * 1.5);
	const double Bn = NCaveBiome2.Sample3(X + 1000, Y - 1000, Z * 1.5);
	if (Z < -18 && Cl.E < -0.25f && A < -0.12) return B(EMCBiome::DeepDark);
	if (Cl.T > 0.3f && Cl.H < -0.05f && Bn < -0.18 && Z > -45 && Z < 36) return B(EMCBiome::SulfurCaves);
	if (A > 0.22 && Cl.H > -0.2f) return B(EMCBiome::LushCaves);
	if (Bn > 0.2 && Cl.C > 0.35f) return B(EMCBiome::DripstoneCaves);
	return 255;
}

double FMCOverworldGen::TerrainNoise3(int32 GX, int32 GY, int32 GZ) const
{
	// sampled on the world-aligned density grid; GZ in world blocks
	return NDetail.Sample3(GX * 1.2, GY * 1.2, GZ * 2.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Proto chunk (terrain + caves + surface + fluids)

TSharedPtr<const FMCProtoChunk> FMCOverworldGen::GetProto(int32 CX, int32 CY) const
{
	const uint64 Key = ((uint64)(uint32)CX << 32) | (uint32)CY;
	{
		FScopeLock L(&Protos->Lock);
		if (const TSharedPtr<const FMCProtoChunk>* P = Protos->Map.Find(Key)) return *P;
	}
	TSharedPtr<const FMCProtoChunk> P = BuildProto(CX, CY);
	{
		FScopeLock L(&Protos->Lock);
		if (const TSharedPtr<const FMCProtoChunk>* Existing = Protos->Map.Find(Key)) return *Existing;
		Protos->Map.Add(Key, P);
		Protos->Order.Add(Key);
		if (Protos->Order.Num() > 320)
		{
			for (int32 i = 0; i < 64; ++i) Protos->Map.Remove(Protos->Order[i]);
			Protos->Order.RemoveAt(0, 64);
		}
	}
	return P;
}

int32 FMCOverworldGen::ProtoSurface(int32 X, int32 Y) const
{
	TSharedPtr<const FMCProtoChunk> P = GetProto(X >> 4, Y >> 4);
	return P->Surface[(X & 15) + (Y & 15) * 16];
}

TSharedPtr<FMCProtoChunk> FMCOverworldGen::BuildProto(int32 CX, int32 CY) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	TSharedPtr<FMCProtoChunk> P = MakeShared<FMCProtoChunk>();
	P->Pos = FMCChunkPos(CX, CY);
	P->Blocks.SetNumZeroed(16 * 16 * MC::WorldHeight);
	const int32 X0 = CX * 16, Y0 = CY * 16;

	// per-column climate (with 1 block border for slope estimation)
	FMCClimate Cols[18 * 18];
	for (int32 y = -1; y <= 16; ++y)
		for (int32 x = -1; x <= 16; ++x)
			Cols[(x + 1) + (y + 1) * 18] = SampleClimate(X0 + x, Y0 + y);
	auto Col = [&](int32 x, int32 y) -> const FMCClimate& { return Cols[(x + 1) + (y + 1) * 18]; };

	// 3D terrain noise grid nodes (5x5 x NZNodes)
	TArray<float> Noise;
	Noise.SetNumUninitialized(5 * 5 * NZNodes);
	float MaxScale = 0.f, MaxH = -1000.f, MinH = 1000.f;
	for (int32 y = 0; y < 16; ++y) for (int32 x = 0; x < 16; ++x) { const FMCClimate& C = Col(x, y); MaxScale = FMath::Max(MaxScale, C.Scale); MaxH = FMath::Max(MaxH, C.BaseHeight); MinH = FMath::Min(MinH, C.BaseHeight); }
	const int32 NoiseZLo = FMath::Max(MC::MinZ, (int32)(MinH - MaxScale * 1.2f) - GridZ);
	const int32 NoiseZHi = FMath::Min(MC::MaxZ, (int32)(MaxH + MaxScale * 1.2f) + GridZ);
	for (int32 gz = 0; gz < NZNodes; ++gz)
	{
		const int32 WZ = MC::MinZ + gz * GridZ;
		for (int32 gy = 0; gy < 5; ++gy)
			for (int32 gx = 0; gx < 5; ++gx)
			{
				float V = 0.f;
				if (WZ >= NoiseZLo - GridZ && WZ <= NoiseZHi + GridZ) V = (float)TerrainNoise3(X0 + gx * GridXY, Y0 + gy * GridXY, WZ);
				Noise[gx + gy * 5 + gz * 25] = V;
			}
	}
	auto NoiseAt = [&](int32 x, int32 y, int32 Z) -> float
	{
		const float fx = x / (float)GridXY, fy = y / (float)GridXY, fz = (Z - MC::MinZ) / (float)GridZ;
		const int32 ix = FMath::Min((int32)fx, 3), iy = FMath::Min((int32)fy, 3), iz = FMath::Min((int32)fz, NZNodes - 2);
		const float tx = fx - ix, ty = fy - iy, tz = fz - iz;
		auto N = [&](int32 a, int32 b, int32 c) { return Noise[(ix + a) + (iy + b) * 5 + (iz + c) * 25]; };
		const float c00 = FMath::Lerp(N(0, 0, 0), N(1, 0, 0), tx), c10 = FMath::Lerp(N(0, 1, 0), N(1, 1, 0), tx);
		const float c01 = FMath::Lerp(N(0, 0, 1), N(1, 0, 1), tx), c11 = FMath::Lerp(N(0, 1, 1), N(1, 1, 1), tx);
		return FMath::Lerp(FMath::Lerp(c00, c10, ty), FMath::Lerp(c01, c11, ty), tz);
	};

	// cave noise grids (5x5 x NCaveZNodes)
	TArray<float> Cheese, Sp1, Sp2, SpW, Nd1, Nd2;
	const int32 NC = 5 * 5 * NCaveZNodes;
	Cheese.SetNumUninitialized(NC); Sp1.SetNumUninitialized(NC); Sp2.SetNumUninitialized(NC); SpW.SetNumUninitialized(25); Nd1.SetNumUninitialized(NC); Nd2.SetNumUninitialized(NC);
	for (int32 gy = 0; gy < 5; ++gy)
		for (int32 gx = 0; gx < 5; ++gx)
			SpW[gx + gy * 5] = (float)NSpagWidth.Sample2(X0 + gx * 4, Y0 + gy * 4);
	const int32 CaveTop = FMath::Min(MC::MaxZ, (int32)MaxH + 4);
	for (int32 gz = 0; gz < NCaveZNodes; ++gz)
	{
		const int32 WZ = MC::MinZ + gz * CaveGridZ;
		for (int32 gy = 0; gy < 5; ++gy)
			for (int32 gx = 0; gx < 5; ++gx)
			{
				const int32 I = gx + gy * 5 + gz * 25;
				if (WZ > CaveTop + CaveGridZ) { Cheese[I] = -1.f; Sp1[I] = Sp2[I] = 1.f; Nd1[I] = Nd2[I] = 1.f; continue; }
				const double WX = X0 + gx * 4, WY = Y0 + gy * 4;
				Cheese[I] = (float)NCheese.Sample3(WX, WY, WZ * 1.6);
				Sp1[I] = (float)NSpag1.Sample3(WX, WY, WZ * 1.3);
				Sp2[I] = (float)NSpag2.Sample3(WX, WY, WZ * 1.3);
				if (WZ < 50) { Nd1[I] = (float)NNoodle1.Sample3(WX, WY, WZ); Nd2[I] = (float)NNoodle2.Sample3(WX, WY, WZ); }
				else { Nd1[I] = Nd2[I] = 1.f; }
			}
	}
	auto CaveAt = [&](const TArray<float>& Arr, int32 x, int32 y, int32 Z) -> float
	{
		const float fx = x / 4.f, fy = y / 4.f, fz = (Z - MC::MinZ) / (float)CaveGridZ;
		const int32 ix = FMath::Min((int32)fx, 3), iy = FMath::Min((int32)fy, 3), iz = FMath::Min((int32)fz, NCaveZNodes - 2);
		const float tx = fx - ix, ty = fy - iy, tz = fz - iz;
		auto N = [&](int32 a, int32 b, int32 c) { return Arr[(ix + a) + (iy + b) * 5 + (iz + c) * 25]; };
		const float c00 = FMath::Lerp(N(0, 0, 0), N(1, 0, 0), tx), c10 = FMath::Lerp(N(0, 1, 0), N(1, 1, 0), tx);
		const float c01 = FMath::Lerp(N(0, 0, 1), N(1, 0, 1), tx), c11 = FMath::Lerp(N(0, 1, 1), N(1, 1, 1), tx);
		return FMath::Lerp(FMath::Lerp(c00, c10, ty), FMath::Lerp(c01, c11, ty), tz);
	};

	for (int32 y = 0; y < 16; ++y)
	{
		for (int32 x = 0; x < 16; ++x)
		{
			const FMCClimate& Cl = Col(x, y);
			const int32 WX = X0 + x, WY = Y0 + y;
			const float H = Cl.BaseHeight, Sc = Cl.Scale;
			const int32 SolidBelow = (int32)FMath::FloorToFloat(H - Sc * 1.1f);
			const int32 AirAbove = (int32)FMath::CeilToFloat(H + Sc * 1.1f);
			const bool bOcean = H < MC::SeaLevel - 1;
			const float SpWidth = 0.055f + 0.035f * (float)FMath::Clamp(SpW[FMath::Min(x / 4, 3) + FMath::Min(y / 4, 3) * 5], -1.f, 1.f);
			const double Entrance = NEntrance.Sample2(WX, WY);
			const uint64 ColHash = MCHash::Hash2(Seed ^ 0xBEDF00Dull, WX, WY);

			int32 Top = MC::MinZ - 1;
			for (int32 Z = FMath::Min(AirAbove, MC::MaxZ); Z >= MC::MinZ; --Z)
			{
				bool bSolid;
				if (Z <= SolidBelow) bSolid = true;
				else if (Z >= AirAbove) bSolid = false;
				else bSolid = (H - Z) + NoiseAt(x, y, Z) * Sc * 1.5f > 0.f;
				if (!bSolid) continue;

				// bedrock floor
				if (Z == MC::MinZ || (Z < MC::MinZ + 5 && (int32)((ColHash >> ((Z - MC::MinZ) * 8)) & 7) < (MC::MinZ + 5 - Z)))
				{
					P->Blocks[ProtoIdx(x, y, Z)] = G.Bedrock;
					continue;
				}

				// caves
				bool bCave = false;
				if (Z > MC::MinZ + 4 && Z < H + 2)
				{
					const float Depth = H - Z;
					const bool bNearSurface = Depth < 5.f;
					// no entrances through a submerged bed, and caves keep 9 blocks under any water body (river,
					// lake, swamp, ocean): Minecraft's aquifer barrier, so generated water cannot leak into caves
					const bool bSubmerged = bOcean || H < MC::SeaLevel - 1;
					const bool bAllowEntrance = Entrance > 0.28 && !bSubmerged;
					if (!bNearSurface || bAllowEntrance)
					{
						if (!bSubmerged || Z < H - 9)
						{
							const float Ch = CaveAt(Cheese, x, y, Z);
							const float CheeseThresh = 0.30f + FMath::Clamp((Z - 10) / 110.f, 0.f, 1.f) * 0.22f + (bNearSurface ? 0.25f : 0.f);
							if (Ch > CheeseThresh) bCave = true;
							else
							{
								const float S1 = CaveAt(Sp1, x, y, Z), S2 = CaveAt(Sp2, x, y, Z);
								if (FMath::Abs(S1) + FMath::Abs(S2) < SpWidth) bCave = true;
								else if (Z < 48)
								{
									const float N1 = CaveAt(Nd1, x, y, Z), N2 = CaveAt(Nd2, x, y, Z);
									if (FMath::Abs(N1) + FMath::Abs(N2) < 0.035f) bCave = true;
								}
							}
						}
					}
				}
				if (bCave)
				{
					if (Z <= -55) P->Blocks[ProtoIdx(x, y, Z)] = G.Lava;
					continue;
				}
				// base stone with deepslate transition
				FMCState Base = G.Stone;
				if (Z < 0 || (Z < 8 && (int32)(ColHash >> (Z * 3) & 7) >= Z)) Base = G.Deepslate;
				P->Blocks[ProtoIdx(x, y, Z)] = Base;
				if (Top == MC::MinZ - 1) Top = Z;
			}
			P->Surface[x + y * 16] = (int16)Top;
		}
	}

	// ---------------------------------------------------------------- surface rules + water
	for (int32 y = 0; y < 16; ++y)
	{
		for (int32 x = 0; x < 16; ++x)
		{
			const FMCClimate& Cl = Col(x, y);
			const int32 WX = X0 + x, WY = Y0 + y;
			const int32 Top = P->Surface[x + y * 16];
			const uint8 Bi = PickSurfaceBiome(Cl, Top);
			P->Biome[x + y * 16] = Bi;
			P->Temp[x + y * 16] = Cl.T;
			P->Humid[x + y * 16] = Cl.H;
			const FMCBiomeDef& BD = FMCBiomes::Get(Bi);
			const EMCBiome E = (EMCBiome)Bi;
			const double SN = NSurface.Sample2(WX, WY);
			const int32 Depth = 3 + (int32)(SN * 2.0 + 0.5);
			// slope (for mountain surfaces)
			const float Slope = FMath::Max(FMath::Abs(Col(x + 1, y).BaseHeight - Col(x - 1, y).BaseHeight), FMath::Abs(Col(x, y + 1).BaseHeight - Col(x, y - 1).BaseHeight));
			const bool bSteep = Slope > 3.2f;

			FMCState TopB = G.Grass, Filler = G.Dirt, Under = 0;
			switch (E)
			{
			case EMCBiome::Desert: TopB = G.Sand; Filler = G.Sand; Under = G.Sandstone; break;
			case EMCBiome::Beach: TopB = G.Sand; Filler = G.Sand; Under = G.Sandstone; break;
			case EMCBiome::SnowyBeach: TopB = G.Sand; Filler = G.Sand; break;
			case EMCBiome::StonyShore: TopB = SN > 0.2 ? G.Gravel : G.Stone; Filler = G.Stone; break;
			case EMCBiome::Badlands: case EMCBiome::ErodedBadlands: TopB = G.RedSand; Filler = G.Terracotta; break;
			case EMCBiome::WoodedBadlands: TopB = Top > 95 ? (SN > 0 ? G.Grass : G.CoarseDirt) : G.RedSand; Filler = Top > 95 ? G.Dirt : G.Terracotta; break;
			case EMCBiome::MushroomFields: TopB = G.Mycelium; break;
			case EMCBiome::MangroveSwamp: TopB = G.Mud; Filler = G.Mud; break;
			case EMCBiome::OldGrowthPineTaiga: case EMCBiome::OldGrowthSpruceTaiga: TopB = SN > 0.25 ? G.Podzol : (SN < -0.35 ? G.CoarseDirt : G.Grass); break;
			case EMCBiome::BambooJungle: TopB = SN > 0.1 ? G.Podzol : G.Grass; break;
			case EMCBiome::WindsweptGravellyHills: TopB = SN > -0.2 ? G.Gravel : G.Grass; Filler = G.Gravel; break;
			case EMCBiome::WindsweptSavanna: TopB = SN > 0.3 ? G.CoarseDirt : G.Grass; break;
			case EMCBiome::IceSpikes: TopB = G.SnowBlock; break;
			case EMCBiome::SnowySlopes: TopB = bSteep ? G.Stone : G.SnowBlock; Filler = bSteep ? G.Stone : G.Dirt; break;
			case EMCBiome::Grove: TopB = G.GrassSnowy; break;
			case EMCBiome::FrozenPeaks: TopB = bSteep ? G.PackedIce : G.SnowBlock; Filler = G.PackedIce; break;
			case EMCBiome::JaggedPeaks: TopB = bSteep ? G.Stone : G.SnowBlock; Filler = G.Stone; break;
			case EMCBiome::StonyPeaks: TopB = SN > 0.3 ? G.Calcite : G.Stone; Filler = G.Stone; break;
			case EMCBiome::WindsweptHills: TopB = bSteep ? G.Stone : G.Grass; Filler = bSteep ? G.Stone : G.Dirt; break;
			case EMCBiome::PaleGarden: TopB = SN > 0.35 ? G.PaleMoss : G.Grass; break;
			default: break;
			}
			if (BD.bSnowy && TopB == G.Grass) TopB = G.GrassSnowy;
			if (Top > 150 + (int32)(Cl.T * 40.f) && TopB == G.Grass) TopB = bSteep ? G.Stone : G.GrassSnowy;

			int32 D = -1;
			bool bFirst = true;
			for (int32 Z = Top; Z >= FMath::Max(MC::MinZ + 1, Top - 70); --Z)
			{
				const int32 I = ProtoIdx(x, y, Z);
				const FMCState S = P->Blocks[I];
				if (S == 0 || S == G.Lava) { if (D >= 0) D = -2; continue; }
				if (S != G.Stone && S != G.Deepslate) { continue; }
				if (D == -2) { if (Z < Top - 8) break; D = -1; bFirst = false; }
				if (D == -1)
				{
					D = 0;
					const bool bUnderwater = Z < MC::SeaLevel - 1;
					if (bUnderwater)
					{
						FMCState Floor = G.Sand;
						const double CN = NClay.Sample2(WX, WY);
						if (BD.bOcean)
						{
							if (E == EMCBiome::DeepOcean || E == EMCBiome::DeepColdOcean || E == EMCBiome::DeepFrozenOcean || E == EMCBiome::ColdOcean || E == EMCBiome::FrozenOcean) Floor = CN > 0.1 ? G.Sand : G.Gravel;
							else Floor = G.Sand;
							if (CN > 0.45) Floor = G.Clay;
						}
						else if (E == EMCBiome::River || E == EMCBiome::FrozenRiver) Floor = CN > 0.35 ? G.Clay : (CN > -0.1 ? G.Sand : G.Gravel);
						else if (E == EMCBiome::Swamp) Floor = CN > 0.3 ? G.Clay : G.Dirt;
						else if (E == EMCBiome::MangroveSwamp) Floor = G.Mud;
						else if (TopB == G.Stone || TopB == G.Gravel) Floor = TopB;
						else if (Z < MC::SeaLevel - 6) Floor = CN > 0.0 ? G.Sand : G.Gravel;
						P->Blocks[I] = Floor;
					}
					else if (bFirst || Z > Top - 8)
					{
						P->Blocks[I] = TopB;
					}
					continue;
				}
				if (D < Depth)
				{
					++D;
					const FMCState Cur = P->Blocks[I + 256]; // block above
					FMCState F = Filler;
					if (Cur == G.Sand && Filler == G.Dirt) F = G.Sand;
					if (Cur == G.Gravel && Filler == G.Dirt) F = G.Gravel;
					if (Cur == G.Clay) F = G.Clay;
					if (Cur == G.Mud) F = G.Mud;
					if (Z < MC::SeaLevel - 1 && Filler == G.Dirt && Cur != G.Dirt && Cur != G.Grass) F = Cur == 0 ? G.Dirt : Cur;
					if (F == G.Terracotta)
					{
						// badlands bands
						const int32 Band = ((Z + (int32)(NBand.Sample2(WX, WY) * 4.0)) % 11 + 11) % 11;
						static const TCHAR* BandBlocks[11] = { TEXT("terracotta"), TEXT("orange_terracotta"), TEXT("terracotta"), TEXT("yellow_terracotta"),
							TEXT("brown_terracotta"), TEXT("terracotta"), TEXT("red_terracotta"), TEXT("white_terracotta"), TEXT("terracotta"), TEXT("light_gray_terracotta"), TEXT("orange_terracotta") };
						F = MCGen::S(BandBlocks[Band]);
					}
					P->Blocks[I] = F;
					continue;
				}
				if (Under && D < Depth + 4) { ++D; P->Blocks[I] = Under; continue; }
				if (Filler == G.Terracotta && Z > 50)
				{
					const int32 Band = ((Z + (int32)(NBand.Sample2(WX, WY) * 4.0)) % 11 + 11) % 11;
					static const TCHAR* BandBlocks2[11] = { TEXT("terracotta"), TEXT("orange_terracotta"), TEXT("terracotta"), TEXT("yellow_terracotta"),
						TEXT("brown_terracotta"), TEXT("terracotta"), TEXT("red_terracotta"), TEXT("white_terracotta"), TEXT("terracotta"), TEXT("light_gray_terracotta"), TEXT("orange_terracotta") };
					P->Blocks[I] = MCGen::S(BandBlocks2[Band]);
					continue;
				}
				if (!bFirst) break;
				if (D >= Depth + 4) break;
				++D;
			}

			// water / ice fill
			int32 WaterTop = MC::MinZ - 1;
			for (int32 Z = MC::SeaLevel - 1; Z > Top; --Z)
			{
				const int32 I = ProtoIdx(x, y, Z);
				if (P->Blocks[I] != 0) break;
				P->Blocks[I] = (Z == MC::SeaLevel - 1 && BD.bFrozenWater && (E != EMCBiome::DeepFrozenOcean || NIceberg.Sample2(WX, WY) > -0.2)) ? G.Ice : G.Water;
				if (WaterTop == MC::MinZ - 1) WaterTop = Z;
			}
			P->WaterTop[x + y * 16] = (int16)WaterTop;
		}
	}

	// aquifer barrier: cave air right beside generated water (inside this chunk) becomes stone, so still water stays
	// still instead of pouring sideways into the cave system the moment the chunk goes live
	for (int32 Z = MC::SeaLevel - 1; Z > MC::MinZ + 4; --Z)
	{
		for (int32 y = 0; y < 16; ++y)
		{
			for (int32 x = 0; x < 16; ++x)
			{
				if (P->Blocks[ProtoIdx(x, y, Z)] != G.Water) continue;
				static const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
				for (int32 d = 0; d < 4; ++d)
				{
					const int32 nx = x + DX[d], ny = y + DY[d];
					if (nx < 0 || ny < 0 || nx > 15 || ny > 15) continue;
					const int32 NI = ProtoIdx(nx, ny, Z);
					// only below the neighbour's own surface: open air above the land is a shoreline, not a cave
					if (P->Blocks[NI] == 0 && Z < P->Surface[nx + ny * 16]) P->Blocks[NI] = Z < 0 ? G.Deepslate : G.Stone;
				}
				// a cave directly under the water column seals with the same barrier
				if (Z - 1 > MC::MinZ && P->Blocks[ProtoIdx(x, y, Z - 1)] == 0) P->Blocks[ProtoIdx(x, y, Z - 1)] = Z - 1 < 0 ? G.Deepslate : G.Stone;
			}
		}
	}
	return P;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ores

void FMCOverworldGen::PlaceOres(FMCGenWriter& W, const FMCProtoChunk& P, int32 OCX, int32 OCY) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0x0E5);
	const int32 X0 = OCX * 16, Y0 = OCY * 16;
	const uint8 Bi = P.Biome[8 + 8 * 16];
	const EMCBiome E = (EMCBiome)Bi;
	const bool bMountain = E == EMCBiome::WindsweptHills || E == EMCBiome::WindsweptForest || E == EMCBiome::WindsweptGravellyHills || E == EMCBiome::JaggedPeaks
		|| E == EMCBiome::FrozenPeaks || E == EMCBiome::StonyPeaks || E == EMCBiome::SnowySlopes || E == EMCBiome::Grove || E == EMCBiome::Meadow || E == EMCBiome::CherryGrove;
	const bool bBadlands = E == EMCBiome::Badlands || E == EMCBiome::ErodedBadlands || E == EMCBiome::WoodedBadlands;

	auto Uniform = [&](int32 Lo, int32 Hi) { return R.Range(Lo, Hi); };
	auto Triangle = [&](int32 Lo, int32 Hi) { return (R.Range(Lo, Hi) + R.Range(Lo, Hi)) / 2; };
	auto Ore = [&](int32 Attempts, int32 Size, FMCState S, FMCState DS, bool bTri, int32 Lo, int32 Hi, float AirSkip = 0.f)
	{
		for (int32 i = 0; i < Attempts; ++i)
		{
			const int32 X = X0 + R.NextInt(16), Y = Y0 + R.NextInt(16);
			const int32 Z = bTri ? Triangle(Lo, Hi) : Uniform(Lo, Hi);
			if (Z < MC::MinZ + 1 || Z > MC::MaxZ) continue;
			MCFeatures::Blob(W, R, FMCBlockPos(X, Y, Z), Size, S, DS, true, AirSkip);
		}
	};
	// stone variety
	Ore(2, 64, G.Granite, 0, false, 0, 60);
	Ore(2, 64, G.Diorite, 0, false, 0, 60);
	Ore(2, 64, G.Andesite, 0, false, 0, 60);
	Ore(2, 64, G.Tuff, G.Tuff, false, -64, 0);
	Ore(4, 33, G.Dirt, 0, false, 0, 160);
	Ore(6, 33, G.Gravel, 0, false, -64, 160);
	Ore(1, 20, G.Clay, 0, false, 0, 60);
	// ores
	Ore(12, 17, G.CoalOre, G.DCoalOre, false, 136, 256);
	Ore(20, 17, G.CoalOre, G.DCoalOre, true, 0, 192);
	Ore(10, 9, G.IronOre, G.DIronOre, true, -24, 56);
	Ore(10, 9, G.IronOre, G.DIronOre, false, -64, 72);
	if (bMountain) Ore(24, 9, G.IronOre, G.DIronOre, true, 80, 320);
	Ore(16, 10, G.CopperOre, G.DCopperOre, true, -16, 112);
	Ore(4, 9, G.GoldOre, G.DGoldOre, true, -64, 32);
	if (bBadlands) Ore(30, 9, G.GoldOre, G.DGoldOre, false, 32, 256);
	Ore(4, 8, G.RedstoneOre, G.DRedstoneOre, false, -64, 15);
	Ore(8, 8, G.RedstoneOre, G.DRedstoneOre, true, -96, -32);
	Ore(2, 7, G.LapisOre, G.DLapisOre, true, -32, 32);
	Ore(4, 7, G.LapisOre, G.DLapisOre, false, -64, 64, 1.f);
	Ore(7, 4, G.DiamondOre, G.DDiamondOre, true, -144, 16, 0.5f);
	if (R.Chance(1.0 / 9.0)) Ore(1, 12, G.DiamondOre, G.DDiamondOre, true, -144, 16, 0.7f);
	if (bMountain) Ore(12, 3, G.EmeraldOre, G.DEmeraldOre, true, -16, 320);
	// geodes
	if (R.Chance(1.0 / 24.0))
	{
		MCFeatures::Geode(W, R, FMCBlockPos(X0 + R.Range(4, 11), Y0 + R.Range(4, 11), R.Range(-50, 30)));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Vegetation / trees (called for the 3x3 neighbourhood; W clips to the target chunk)

void FMCOverworldGen::PlaceFeatures(FMCGenWriter& W, int32 OCX, int32 OCY) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	TSharedPtr<const FMCProtoChunk> PP = GetProto(OCX, OCY);
	const FMCProtoChunk& P = *PP;
	FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0xF3A7);
	const int32 X0 = OCX * 16, Y0 = OCY * 16;
	const uint8 CenterBiome = P.Biome[8 + 8 * 16];

	auto Surf = [&](int32 lx, int32 ly) { return (int32)P.Surface[lx + ly * 16]; };
	auto BiomeAtL = [&](int32 lx, int32 ly) { return (EMCBiome)P.Biome[lx + ly * 16]; };
	auto Ground = [&](int32 lx, int32 ly) { return P.Get(lx, ly, Surf(lx, ly)); };
	auto Dry = [&](int32 lx, int32 ly) { const int32 Z = Surf(lx, ly); return P.Get(lx, ly, Z + 1) == 0; };
	auto IsSoil = [&](FMCState S) { return S == G.Grass || S == G.GrassSnowy || S == G.Dirt || S == G.Podzol || S == G.CoarseDirt || S == G.Mycelium || S == G.Mud || S == G.PaleMoss || S == G.MossBlock; };

	auto PlaceTree = [&](EMCTree T, int32 lx, int32 ly, bool bRequireSoil = true)
	{
		const int32 Z = Surf(lx, ly);
		if (!Dry(lx, ly) && T != EMCTree::Mangrove) return;
		const FMCState Gr = Ground(lx, ly);
		if (bRequireSoil && !IsSoil(Gr) && T != EMCTree::Mangrove) return;
		if (T == EMCTree::Mangrove && Z < MC::SeaLevel - 4) return;
		FMCRandom TR(MCHash::Hash3(Seed ^ 0x7EE5, X0 + lx, Y0 + ly, Z));
		// grass under trunks becomes dirt
		if (Gr == G.Grass || Gr == G.GrassSnowy) W.Set(X0 + lx, Y0 + ly, Z, G.Dirt);
		MCFeatures::Tree(W, TR, T, FMCBlockPos(X0 + lx, Y0 + ly, Z + 1));
	};
	auto PlacePlant = [&](FMCState Plant, int32 lx, int32 ly, FMCState Top = 0)
	{
		const int32 Z = Surf(lx, ly);
		if (!Dry(lx, ly)) return;
		const FMCState Gr = Ground(lx, ly);
		if (!IsSoil(Gr) && Gr != G.Sand && Gr != G.RedSand) return;
		const int32 X = X0 + lx, Y = Y0 + ly;
		if (W.Get(X, Y, Z + 1) != 0 && W.InsideXY(X, Y)) return;
		W.Set(X, Y, Z + 1, Plant);
		if (Top) W.Set(X, Y, Z + 2, Top);
	};

	// --- tree counts per biome
	struct FTreeMix { EMCTree T; float W; };
	TArray<FTreeMix, TInlineAllocator<4>> Mix;
	float Trees = 0.f;
	float GrassDensity = 0.35f;
	int32 FlowerCount = 1;
	const EMCBiome E = (EMCBiome)CenterBiome;
	switch (E)
	{
	case EMCBiome::Plains: Trees = R.Chance(0.12) ? 1.f : 0.f; Mix = { { EMCTree::Oak, 1.f } }; GrassDensity = 0.55f; FlowerCount = 2; break;
	case EMCBiome::SunflowerPlains: Trees = 0.f; GrassDensity = 0.55f; FlowerCount = 2; break;
	case EMCBiome::Forest: Trees = 10.f; Mix = { { EMCTree::Oak, 0.7f }, { EMCTree::Birch, 0.2f }, { EMCTree::FancyOak, 0.1f } }; GrassDensity = 0.2f; FlowerCount = 2; break;
	case EMCBiome::FlowerForest: Trees = 6.f; Mix = { { EMCTree::Oak, 0.6f }, { EMCTree::Birch, 0.3f }, { EMCTree::FancyOak, 0.1f } }; GrassDensity = 0.15f; FlowerCount = 18; break;
	case EMCBiome::BirchForest: Trees = 10.f; Mix = { { EMCTree::Birch, 1.f } }; GrassDensity = 0.2f; FlowerCount = 2; break;
	case EMCBiome::OldGrowthBirchForest: Trees = 10.f; Mix = { { EMCTree::TallBirch, 0.8f }, { EMCTree::Birch, 0.2f } }; GrassDensity = 0.2f; FlowerCount = 2; break;
	case EMCBiome::DarkForest: Trees = 0.f; GrassDensity = 0.1f; FlowerCount = 1; break;
	case EMCBiome::PaleGarden: Trees = 0.f; GrassDensity = 0.08f; FlowerCount = 0; break;
	case EMCBiome::Taiga: Trees = 10.f; Mix = { { EMCTree::Spruce, 0.66f }, { EMCTree::Pine, 0.34f } }; GrassDensity = 0.25f; FlowerCount = 0; break;
	case EMCBiome::SnowyTaiga: Trees = 9.f; Mix = { { EMCTree::Spruce, 0.66f }, { EMCTree::Pine, 0.34f } }; GrassDensity = 0.08f; FlowerCount = 0; break;
	case EMCBiome::OldGrowthPineTaiga: case EMCBiome::OldGrowthSpruceTaiga: Trees = 10.f; Mix = { { EMCTree::MegaSpruce, 0.33f }, { EMCTree::Spruce, 0.5f }, { EMCTree::Pine, 0.17f } }; GrassDensity = 0.3f; FlowerCount = 0; break;
	case EMCBiome::SnowyPlains: Trees = R.Chance(0.1) ? 1.f : 0.f; Mix = { { EMCTree::Spruce, 1.f } }; GrassDensity = 0.05f; FlowerCount = 0; break;
	case EMCBiome::Savanna: case EMCBiome::SavannaPlateau: Trees = 1.5f; Mix = { { EMCTree::Acacia, 0.8f }, { EMCTree::Oak, 0.2f } }; GrassDensity = 0.7f; FlowerCount = 1; break;
	case EMCBiome::WindsweptSavanna: Trees = 1.f; Mix = { { EMCTree::Acacia, 1.f } }; GrassDensity = 0.5f; break;
	case EMCBiome::Jungle: Trees = 12.f; Mix = { { EMCTree::MegaJungle, 0.12f }, { EMCTree::Jungle, 0.4f }, { EMCTree::JungleBush, 0.38f }, { EMCTree::FancyOak, 0.1f } }; GrassDensity = 0.5f; FlowerCount = 1; break;
	case EMCBiome::SparseJungle: Trees = 3.f; Mix = { { EMCTree::Jungle, 0.5f }, { EMCTree::JungleBush, 0.5f } }; GrassDensity = 0.5f; break;
	case EMCBiome::BambooJungle: Trees = 4.f; Mix = { { EMCTree::Jungle, 0.4f }, { EMCTree::JungleBush, 0.6f } }; GrassDensity = 0.4f; break;
	case EMCBiome::Swamp: Trees = 2.f; Mix = { { EMCTree::SwampOak, 1.f } }; GrassDensity = 0.3f; FlowerCount = 1; break;
	case EMCBiome::MangroveSwamp: Trees = 10.f; Mix = { { EMCTree::Mangrove, 1.f } }; GrassDensity = 0.f; FlowerCount = 0; break;
	case EMCBiome::Meadow: Trees = R.Chance(0.1) ? 1.f : 0.f; Mix = { { EMCTree::Oak, 0.5f }, { EMCTree::Birch, 0.5f } }; GrassDensity = 0.6f; FlowerCount = 10; break;
	case EMCBiome::CherryGrove: Trees = 3.f; Mix = { { EMCTree::Cherry, 1.f } }; GrassDensity = 0.35f; FlowerCount = 0; break;
	case EMCBiome::Grove: Trees = 8.f; Mix = { { EMCTree::Spruce, 1.f } }; GrassDensity = 0.f; FlowerCount = 0; break;
	case EMCBiome::WindsweptForest: Trees = 4.f; Mix = { { EMCTree::Spruce, 0.6f }, { EMCTree::Oak, 0.4f } }; GrassDensity = 0.2f; break;
	case EMCBiome::WindsweptHills: Trees = R.Chance(0.3) ? 1.f : 0.f; Mix = { { EMCTree::Spruce, 0.6f }, { EMCTree::Oak, 0.4f } }; GrassDensity = 0.2f; break;
	case EMCBiome::WoodedBadlands: Trees = 4.f; Mix = { { EMCTree::Oak, 1.f } }; GrassDensity = 0.1f; FlowerCount = 0; break;
	case EMCBiome::MushroomFields: Trees = 0.6f; Mix = { { EMCTree::RedMushroom, 0.5f }, { EMCTree::BrownMushroom, 0.5f } }; GrassDensity = 0.f; FlowerCount = 0; break;
	case EMCBiome::Desert: case EMCBiome::Badlands: case EMCBiome::ErodedBadlands: GrassDensity = 0.f; FlowerCount = 0; break;
	case EMCBiome::IceSpikes: case EMCBiome::JaggedPeaks: case EMCBiome::FrozenPeaks: case EMCBiome::StonyPeaks: case EMCBiome::SnowySlopes: GrassDensity = 0.f; FlowerCount = 0; break;
	default: GrassDensity = BiomeAtL(8, 8) == EMCBiome::River ? 0.1f : 0.f; FlowerCount = 0; break;
	}

	// dark forest / pale garden canopy on a grid
	if (E == EMCBiome::DarkForest || E == EMCBiome::PaleGarden)
	{
		for (int32 gy = 0; gy < 4; ++gy)
			for (int32 gx = 0; gx < 4; ++gx)
			{
				const int32 lx = gx * 4 + R.NextInt(3), ly = gy * 4 + R.NextInt(3);
				const float Roll = R.NextFloat();
				if (E == EMCBiome::DarkForest)
				{
					if (Roll < 0.025f) PlaceTree(R.NextBool() ? EMCTree::RedMushroom : EMCTree::BrownMushroom, lx, ly);
					else if (Roll < 0.7f) PlaceTree(EMCTree::DarkOak, FMath::Min(lx, 14), FMath::Min(ly, 14));
					else if (Roll < 0.85f) PlaceTree(EMCTree::Birch, lx, ly);
					else PlaceTree(EMCTree::Oak, lx, ly);
				}
				else if (Roll < 0.8f) PlaceTree(EMCTree::PaleOak, FMath::Min(lx, 14), FMath::Min(ly, 14));
			}
	}

	// generic trees
	int32 NumTrees = (int32)Trees;
	if (R.NextFloat() < Trees - NumTrees) ++NumTrees;
	for (int32 i = 0; i < NumTrees; ++i)
	{
		if (Mix.Num() == 0) break;
		const int32 lx = R.NextInt(16), ly = R.NextInt(16);
		float Roll = R.NextFloat();
		EMCTree T = Mix.Last().T;
		for (const FTreeMix& M : Mix) { if (Roll < M.W) { T = M.T; break; } Roll -= M.W; }
		if ((T == EMCTree::MegaSpruce || T == EMCTree::MegaJungle) && (lx > 14 || ly > 14)) continue;
		PlaceTree(T, lx, ly);
	}

	// grass & ferns
	const int32 GrassTries = (int32)(GrassDensity * 64.f);
	for (int32 i = 0; i < GrassTries; ++i)
	{
		const int32 lx = R.NextInt(16), ly = R.NextInt(16);
		const EMCBiome LB = BiomeAtL(lx, ly);
		const bool bTaiga = LB == EMCBiome::Taiga || LB == EMCBiome::SnowyTaiga || LB == EMCBiome::OldGrowthPineTaiga || LB == EMCBiome::OldGrowthSpruceTaiga || LB == EMCBiome::Jungle;
		const float Roll = R.NextFloat();
		if (bTaiga && Roll < 0.4f) { if (R.Chance(0.15)) PlacePlant(G.LargeFern, lx, ly, G.LargeFernTop); else PlacePlant(G.Fern, lx, ly); }
		else if (Roll < 0.1f) PlacePlant(G.TallGrass, lx, ly, G.TallGrassTop);
		else if (LB == EMCBiome::Plains && Roll < 0.13f) PlacePlant(G.Bush, lx, ly);
		else PlacePlant(G.ShortGrass, lx, ly);
	}

	// flowers per biome
	for (int32 i = 0; i < FlowerCount; ++i)
	{
		const int32 lx = R.NextInt(16), ly = R.NextInt(16);
		const EMCBiome LB = BiomeAtL(lx, ly);
		FMCState F = R.NextBool() ? G.Dandelion : G.Poppy;
		switch (LB)
		{
		case EMCBiome::FlowerForest:
		{
			const FMCState Fs[] = { G.Dandelion, G.Poppy, G.Allium, G.AzureBluet, G.RedTulip, G.OrangeTulip, G.WhiteTulip, G.PinkTulip, G.OxeyeDaisy, G.Cornflower, G.LilyOfValley };
			F = Fs[R.NextInt(UE_ARRAY_COUNT(Fs))];
			if (R.Chance(0.1)) { PlacePlant(R.NextBool() ? G.Lilac : G.RoseBush, lx, ly, R.NextBool() ? G.LilacTop : G.RoseBushTop); continue; }
			break;
		}
		case EMCBiome::Meadow:
		{
			const FMCState Fs[] = { G.Dandelion, G.Poppy, G.Allium, G.AzureBluet, G.OxeyeDaisy, G.Cornflower };
			F = Fs[R.NextInt(UE_ARRAY_COUNT(Fs))];
			if (R.Chance(0.3)) F = G.Wildflowers;
			break;
		}
		case EMCBiome::Plains:
		{
			const FMCState Fs[] = { G.Dandelion, G.Poppy, G.AzureBluet, G.OxeyeDaisy, G.Cornflower, G.RedTulip, G.WhiteTulip };
			F = Fs[R.NextInt(UE_ARRAY_COUNT(Fs))];
			break;
		}
		case EMCBiome::Swamp: F = G.BlueOrchid; break;
		case EMCBiome::Forest: if (R.Chance(0.2)) { PlacePlant(G.Peony, lx, ly, G.PeonyTop); continue; } F = R.Chance(0.2) ? G.LilyOfValley : F; break;
		case EMCBiome::BirchForest: case EMCBiome::OldGrowthBirchForest: F = R.Chance(0.3) ? G.Wildflowers : F; break;
		default: break;
		}
		// flowers grow in small patches
		for (int32 k = 0; k < 4; ++k) PlacePlant(F, FMath::Clamp(lx + R.Range(-2, 2), 0, 15), FMath::Clamp(ly + R.Range(-2, 2), 0, 15));
	}
	if (E == EMCBiome::SunflowerPlains) for (int32 i = 0; i < 10; ++i) PlacePlant(G.Sunflower, R.NextInt(16), R.NextInt(16), G.SunflowerTop);
	if (E == EMCBiome::CherryGrove) for (int32 i = 0; i < 12; ++i) PlacePlant(G.PinkPetals, R.NextInt(16), R.NextInt(16));
	if (E == EMCBiome::PaleGarden)
	{
		for (int32 i = 0; i < 10; ++i) PlacePlant(G.PaleMossCarpet, R.NextInt(16), R.NextInt(16));
		for (int32 i = 0; i < 2; ++i) PlacePlant(G.ClosedEyeblossom, R.NextInt(16), R.NextInt(16));
	}
	if (E == EMCBiome::Forest || E == EMCBiome::BirchForest || E == EMCBiome::DarkForest) for (int32 i = 0; i < 3; ++i) PlacePlant(G.LeafLitter, R.NextInt(16), R.NextInt(16));
	if (E == EMCBiome::Swamp || E == EMCBiome::MangroveSwamp || E == EMCBiome::Plains) { if (R.Chance(0.2)) PlacePlant(G.FireflyBush, R.NextInt(16), R.NextInt(16)); }

	// desert & badlands
	if (E == EMCBiome::Desert || E == EMCBiome::Badlands || E == EMCBiome::ErodedBadlands || E == EMCBiome::WoodedBadlands)
	{
		for (int32 i = 0; i < 2; ++i) PlacePlant(G.DeadBush, R.NextInt(16), R.NextInt(16));
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 lx = R.NextInt(16), ly = R.NextInt(16);
			const int32 Z = Surf(lx, ly);
			if (!Dry(lx, ly)) continue;
			const FMCState Gr = Ground(lx, ly);
			if (Gr != G.Sand && Gr != G.RedSand) continue;
			const int32 H = 1 + R.NextInt(3);
			for (int32 k = 1; k <= H; ++k) W.Set(X0 + lx, Y0 + ly, Z + k, G.Cactus);
		}
		if (E == EMCBiome::Desert && R.Chance(1.0 / 400.0))
		{
			const int32 lx = R.Range(3, 12), ly = R.Range(3, 12);
			MCFeatures::DesertWell(W, FMCBlockPos(X0 + lx, Y0 + ly, Surf(lx, ly) + 1));
		}
	}
	// taiga sweet berries, pumpkins
	if ((E == EMCBiome::Taiga || E == EMCBiome::SnowyTaiga || E == EMCBiome::OldGrowthSpruceTaiga) && R.Chance(0.25)) for (int32 i = 0; i < 3; ++i) PlacePlant(G.SweetBerries, R.NextInt(16), R.NextInt(16));
	if (R.Chance(1.0 / 60.0) && !FMCBiomes::Get(CenterBiome).bOcean && E != EMCBiome::Desert) for (int32 i = 0; i < 5; ++i) PlacePlant(G.Pumpkin, R.NextInt(16), R.NextInt(16));
	if ((E == EMCBiome::Jungle || E == EMCBiome::SparseJungle) && R.Chance(0.3)) for (int32 i = 0; i < 4; ++i) PlacePlant(G.Melon, R.NextInt(16), R.NextInt(16));
	// bamboo
	if (E == EMCBiome::BambooJungle || (E == EMCBiome::Jungle && R.Chance(0.2)))
	{
		const int32 N = E == EMCBiome::BambooJungle ? 40 : 6;
		for (int32 i = 0; i < N; ++i)
		{
			const int32 lx = R.NextInt(16), ly = R.NextInt(16);
			if (!Dry(lx, ly) || !IsSoil(Ground(lx, ly))) continue;
			const int32 Z = Surf(lx, ly);
			const int32 H = 6 + R.NextInt(10);
			for (int32 k = 1; k <= H; ++k) W.Set(X0 + lx, Y0 + ly, Z + k, k >= H - 2 ? G.BambooLeaves : G.Bamboo);
		}
	}
	// mushroom fields / swamps small mushrooms
	if (E == EMCBiome::MushroomFields || E == EMCBiome::Swamp || E == EMCBiome::DarkForest || E == EMCBiome::OldGrowthSpruceTaiga)
		for (int32 i = 0; i < 2; ++i) if (R.Chance(0.5)) PlacePlant(R.NextBool() ? G.RedMushroom : G.BrownMushroom, R.NextInt(16), R.NextInt(16));
	// boulders
	if ((E == EMCBiome::OldGrowthPineTaiga || E == EMCBiome::OldGrowthSpruceTaiga) && R.Chance(0.35))
	{
		const int32 lx = R.Range(2, 13), ly = R.Range(2, 13);
		MCFeatures::Boulder(W, R, FMCBlockPos(X0 + lx, Y0 + ly, Surf(lx, ly)), G.MossyCobble);
	}
	// ice spikes
	if (E == EMCBiome::IceSpikes && R.Chance(0.4))
	{
		const int32 lx = R.Range(2, 13), ly = R.Range(2, 13);
		MCFeatures::IceSpike(W, R, FMCBlockPos(X0 + lx, Y0 + ly, Surf(lx, ly)));
	}

	// water plants: sugar cane on banks, seagrass/kelp/coral in oceans, lily pads in swamps
	for (int32 i = 0; i < 10; ++i)
	{
		const int32 lx = R.Range(1, 14), ly = R.Range(1, 14);
		const int32 Z = Surf(lx, ly);
		const EMCBiome LB = BiomeAtL(lx, ly);
		const FMCState Above = P.Get(lx, ly, Z + 1);
		if (Above == 0)
		{
			// sugar cane next to water
			const FMCState Gr = Ground(lx, ly);
			if ((Gr == G.Grass || Gr == G.Sand || Gr == G.Dirt || Gr == G.RedSand) && Z == MC::SeaLevel - 1 + 0)
			{
				bool bWater = false;
				const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
				for (int32 d = 0; d < 4; ++d) if (P.Get(lx + DX[d], ly + DY[d], Z) == G.Water) bWater = true;
				if (bWater && R.Chance(0.6)) for (int32 k = 1; k <= 1 + R.NextInt(3); ++k) W.Set(X0 + lx, Y0 + ly, Z + k, G.SugarCane);
			}
			continue;
		}
		if (Above != G.Water) continue;
		const int32 WaterDepth = P.WaterTop[lx + ly * 16] - Z;
		if ((LB == EMCBiome::Swamp || LB == EMCBiome::MangroveSwamp) && WaterDepth <= 2 && R.Chance(0.5))
		{
			W.Set(X0 + lx, Y0 + ly, P.WaterTop[lx + ly * 16] + 1, G.LilyPad);
			continue;
		}
		if (!FMCBiomes::Get((uint8)LB).bOcean && LB != EMCBiome::River) continue;
		if (LB == EMCBiome::WarmOcean)
		{
			if (R.Chance(0.35)) MCFeatures::CoralReef(W, R, FMCBlockPos(X0 + lx, Y0 + ly, Z + 1));
			else if (R.Chance(0.3)) W.Set(X0 + lx, Y0 + ly, Z + 1, G.SeaPickle);
			continue;
		}
		const bool bKelp = (LB == EMCBiome::Ocean || LB == EMCBiome::DeepOcean || LB == EMCBiome::ColdOcean || LB == EMCBiome::DeepColdOcean || LB == EMCBiome::LukewarmOcean || LB == EMCBiome::DeepLukewarmOcean) && WaterDepth > 4;
		if (bKelp && R.Chance(0.5))
		{
			const int32 H = FMath::Min(WaterDepth - 1, 4 + R.NextInt(12));
			for (int32 k = 1; k <= H; ++k) W.Set(X0 + lx, Y0 + ly, Z + k, k == H ? G.Kelp : G.KelpPlant);
		}
		else if (WaterDepth >= 2 && R.Chance(0.3)) { W.Set(X0 + lx, Y0 + ly, Z + 1, G.TallSeagrass); W.Set(X0 + lx, Y0 + ly, Z + 2, G.TallSeagrassTop); }
		else W.Set(X0 + lx, Y0 + ly, Z + 1, G.Seagrass);
	}
	// icebergs
	if ((E == EMCBiome::FrozenOcean || E == EMCBiome::DeepFrozenOcean) && R.Chance(0.08))
	{
		MCFeatures::Iceberg(W, R, FMCBlockPos(X0 + R.Range(3, 12), Y0 + R.Range(3, 12), MC::SeaLevel - 1));
	}
	// lakes
	if (R.Chance(1.0 / 50.0) && !FMCBiomes::Get(CenterBiome).bOcean)
	{
		const int32 lx = R.Range(4, 11), ly = R.Range(4, 11);
		const int32 Z = Surf(lx, ly);
		if (Dry(lx, ly) && Z > MC::SeaLevel) MCFeatures::LavaLake(W, R, FMCBlockPos(X0 + lx, Y0 + ly, Z), (FMCBiomes::Get(CenterBiome).bDry && R.Chance(0.5)) ? G.Lava : G.Water);
	}
	if (R.Chance(1.0 / 10.0))
	{
		MCFeatures::LavaLake(W, R, FMCBlockPos(X0 + R.Range(4, 11), Y0 + R.Range(4, 11), R.Range(-50, 30)), G.Lava);
	}
	// fossils
	if ((E == EMCBiome::Desert || E == EMCBiome::Swamp) && R.Chance(1.0 / 64.0))
	{
		MCFeatures::Fossil(W, R, FMCBlockPos(X0 + 2, Y0 + 2, R.Range(20, 50)), false);
	}
	// springs in cave walls: one attempt per chunk (each live spring washes out the vegetation along its stream,
	// so three per chunk turned every loaded area into a field of floating plant drops); hillside springs above
	// sea level are rarer still
	for (int32 i = 0; i < 1; ++i)
	{
		const int32 lx = R.Range(1, 14), ly = R.Range(1, 14), Z = R.Chance(0.25) ? R.Range(MC::SeaLevel, 100) : R.Range(-40, MC::SeaLevel - 1);
		if (Z >= Surf(lx, ly) - 2) continue;
		if (P.Get(lx, ly, Z) == G.Stone || P.Get(lx, ly, Z) == G.Deepslate)
		{
			const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
			int32 Air = 0, Solid = 0;
			for (int32 d = 0; d < 4; ++d) { const FMCState N = P.Get(lx + DX[d], ly + DY[d], Z); if (N == 0) ++Air; else ++Solid; }
			if (Air == 1 && P.Get(lx, ly, Z + 1) != 0 && P.Get(lx, ly, Z - 1) != 0)
			{
				const FMCState Fl = (Z < 0 && R.Chance(0.4)) ? G.Lava : G.Water;
				W.Set(X0 + lx, Y0 + ly, Z, Fl);
				W.AddTick(X0 + lx, Y0 + ly, Z);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Cave biome decoration (target chunk only)

void FMCOverworldGen::PlaceCaveDecor(FMCGenWriter& W, const FMCProtoChunk& P) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	FMCChunk& C = W.C;
	FMCRandom R = MCGen::ChunkRandom(Seed, C.Pos.X, C.Pos.Y, 0xCA7E);
	const int32 X0 = W.X0, Y0 = W.Y0;
	for (int32 ly = 0; ly < 16; ++ly)
	{
		for (int32 lx = 0; lx < 16; ++lx)
		{
			const int32 Surf = P.Surface[lx + ly * 16];
			const int32 WX = X0 + lx, WY = Y0 + ly;
			for (int32 Z = MC::MinZ + 5; Z < Surf - 6; ++Z)
			{
				const FMCState S = C.Get(lx, ly, Z);
				const uint8 Bi = C.GetBiome(lx, ly, Z);
				if (Bi == 255 || !FMCBiomes::Get(Bi).bCave)
				{
					// generic caves: glow lichen & hanging roots occasionally
					if (S == 0 && R.Chance(0.004))
					{
						for (int32 f = 0; f < 6; ++f)
						{
							const FIntVector& D = MC::FaceDir[f];
							const FMCState N = W.Get(WX + D.X, WY + D.Y, Z + D.Z);
							if (N != 0 && (FMCBlocks::Info(N).Flags & MCB_Stone)) { W.Set(WX, WY, Z, FMCBlocks::GetByState(G.GlowLichen).State((uint8)(1 << f))); break; }
						}
					}
					continue;
				}
				const EMCBiome E = (EMCBiome)Bi;
				const FMCState Below = Z > MC::MinZ ? C.Get(lx, ly, Z - 1) : 0;
				const FMCState Above = C.Get(lx, ly, Z + 1);
				const bool bFloor = S == 0 && Below != 0 && FMCBlocks::IsOpaque(Below);
				const bool bCeil = S == 0 && Above != 0 && FMCBlocks::IsOpaque(Above);
				switch (E)
				{
				case EMCBiome::LushCaves:
					if (bFloor)
					{
						W.Set(WX, WY, Z - 1, R.Chance(0.1) ? G.Clay : G.MossBlock);
						const float Rl = R.NextFloat();
						if (Rl < 0.25f) W.Set(WX, WY, Z, G.MossCarpet);
						else if (Rl < 0.32f) W.Set(WX, WY, Z, G.ShortGrass);
						else if (Rl < 0.35f) W.Set(WX, WY, Z, G.Azalea);
						else if (Rl < 0.37f) W.Set(WX, WY, Z, G.FloweringAzalea);
						else if (Rl < 0.40f) W.Set(WX, WY, Z, G.SmallDripleaf);
					}
					else if (bCeil)
					{
						W.Set(WX, WY, Z + 1, G.MossBlock);
						const float Rl = R.NextFloat();
						if (Rl < 0.12f)
						{
							const int32 Len = 1 + R.NextInt(6);
							for (int32 k = 0; k < Len && C.Get(lx, ly, Z - k) == 0; ++k) C.SetRaw(lx, ly, Z - k, R.Chance(0.25) ? G.CaveVinesLit : G.CaveVines);
						}
						else if (Rl < 0.13f) W.Set(WX, WY, Z, G.SporeBlossom);
					}
					break;
				case EMCBiome::DripstoneCaves:
					if (bFloor && R.Chance(0.4)) { W.Set(WX, WY, Z - 1, G.DripstoneBlock); if (R.Chance(0.25)) for (int32 k = 0; k < 1 + R.NextInt(3); ++k) W.SetSoft(WX, WY, Z + k, G.PointedDripUp); }
					else if (bCeil && R.Chance(0.4)) { W.Set(WX, WY, Z + 1, G.DripstoneBlock); if (R.Chance(0.3)) for (int32 k = 0; k < 1 + R.NextInt(4); ++k) if (C.Get(lx, ly, Z - k) == 0) C.SetRaw(lx, ly, Z - k, G.PointedDripDown); }
					else if (S == G.Stone && R.Chance(0.05)) C.SetRaw(lx, ly, Z, G.DripstoneBlock);
					break;
				case EMCBiome::DeepDark:
					if (bFloor)
					{
						const double N = MCNoiseUtil::Hash01(0x5C, WX / 3, WY / 3, Z / 3);
						if (N < 0.7) W.Set(WX, WY, Z - 1, G.Sculk);
						const float Rl = R.NextFloat();
						if (Rl < 0.01f) W.Set(WX, WY, Z, G.SculkSensor);
						else if (Rl < 0.013f) W.Set(WX, WY, Z, G.SculkShrieker);
						else if (Rl < 0.015f) W.Set(WX, WY, Z - 1, G.SculkCatalyst);
						else if (Rl < 0.06f) W.Set(WX, WY, Z, G.SculkVein);
					}
					else if (bCeil && R.Chance(0.2)) W.Set(WX, WY, Z + 1, G.Sculk);
					break;
				case EMCBiome::SulfurCaves:
				{
					// banded sulfur / cinnabar walls
					if (S != 0 && (FMCBlocks::Info(S).Flags & MCB_Stone))
					{
						const double Band = NSulfurBand.Sample3(WX, WY, Z * 3.0);
						if (Band > 0.18) C.SetRaw(lx, ly, Z, G.Sulfur);
						else if (Band < -0.25) C.SetRaw(lx, ly, Z, G.Cinnabar);
					}
					else if (bFloor)
					{
						const float Rl = R.NextFloat();
						if (Rl < 0.08f) for (int32 k = 0; k < 1 + R.NextInt(3); ++k) W.SetSoft(WX, WY, Z + k, G.SulfurSpikeUp);
						else if (Rl < 0.1f) W.Set(WX, WY, Z - 1, G.PotentSulfur);
						else if (Rl < 0.16f && Below != G.Water) { W.Set(WX, WY, Z - 1, G.Water); }
					}
					else if (bCeil && R.Chance(0.1)) for (int32 k = 0; k < 1 + R.NextInt(3); ++k) if (C.Get(lx, ly, Z - k) == 0) C.SetRaw(lx, ly, Z - k, G.SulfurSpikeDown);
					break;
				}
				default: break;
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void FMCOverworldGen::SpawnInitialAnimals(FMCGenWriter& W, const FMCProtoChunk& P) const
{
	FMCRandom R = MCGen::ChunkRandom(Seed, W.C.Pos.X, W.C.Pos.Y, 0xA417);
	if (!R.Chance(0.1)) return;
	const int32 lx = R.Range(2, 13), ly = R.Range(2, 13);
	const int32 Z = P.Surface[lx + ly * 16];
	if (P.Get(lx, ly, Z + 1) != 0) return;
	const EMCBiome E = (EMCBiome)P.Biome[lx + ly * 16];
	FName Mob = NAME_None;
	int32 Count = 2 + R.NextInt(3);
	const TCHAR* Farm[] = { TEXT("pig"), TEXT("cow"), TEXT("sheep"), TEXT("chicken") };
	switch (E)
	{
	case EMCBiome::Plains: case EMCBiome::SunflowerPlains: Mob = R.Chance(0.15) ? FName(TEXT("horse")) : FName(Farm[R.NextInt(4)]); break;
	case EMCBiome::Forest: case EMCBiome::FlowerForest: case EMCBiome::BirchForest: case EMCBiome::DarkForest: case EMCBiome::Meadow: case EMCBiome::CherryGrove:
		Mob = R.Chance(0.1) ? FName(TEXT("wolf")) : FName(Farm[R.NextInt(4)]);
		if (E == EMCBiome::Meadow && R.Chance(0.3)) Mob = TEXT("donkey");
		if (E == EMCBiome::FlowerForest && R.Chance(0.3)) Mob = TEXT("rabbit");
		break;
	case EMCBiome::Taiga: case EMCBiome::OldGrowthPineTaiga: case EMCBiome::OldGrowthSpruceTaiga: Mob = R.Chance(0.3) ? FName(TEXT("wolf")) : (R.Chance(0.4) ? FName(TEXT("fox")) : FName(Farm[R.NextInt(4)])); break;
	case EMCBiome::SnowyTaiga: case EMCBiome::SnowyPlains: case EMCBiome::IceSpikes: Mob = R.Chance(0.3) ? FName(TEXT("polar_bear")) : (R.Chance(0.5) ? FName(TEXT("rabbit")) : FName(TEXT("fox"))); Count = 1 + R.NextInt(2); break;
	case EMCBiome::Savanna: case EMCBiome::SavannaPlateau: case EMCBiome::WindsweptSavanna: Mob = R.Chance(0.3) ? FName(TEXT("horse")) : (R.Chance(0.3) ? FName(TEXT("llama")) : (R.Chance(0.3) ? FName(TEXT("armadillo")) : FName(Farm[R.NextInt(4)]))); break;
	case EMCBiome::Desert: Mob = R.Chance(0.5) ? FName(TEXT("rabbit")) : FName(TEXT("camel")); Count = 1 + R.NextInt(2); break;
	case EMCBiome::Jungle: case EMCBiome::SparseJungle: case EMCBiome::BambooJungle: Mob = R.Chance(0.4) ? FName(TEXT("parrot")) : (R.Chance(0.5) ? FName(TEXT("ocelot")) : FName(TEXT("panda"))); Count = 1 + R.NextInt(2); if (E == EMCBiome::BambooJungle) Mob = TEXT("panda"); break;
	case EMCBiome::Swamp: case EMCBiome::MangroveSwamp: Mob = TEXT("frog"); break;
	case EMCBiome::MushroomFields: Mob = TEXT("mooshroom"); break;
	case EMCBiome::Grove: case EMCBiome::SnowySlopes: case EMCBiome::JaggedPeaks: case EMCBiome::FrozenPeaks: case EMCBiome::StonyPeaks: Mob = TEXT("goat"); break;
	case EMCBiome::WindsweptHills: case EMCBiome::WindsweptForest: Mob = R.Chance(0.4) ? FName(TEXT("llama")) : FName(Farm[R.NextInt(4)]); break;
	case EMCBiome::Beach: Mob = TEXT("turtle"); break;
	case EMCBiome::Badlands: case EMCBiome::ErodedBadlands: case EMCBiome::WoodedBadlands: Mob = TEXT("armadillo"); Count = 1 + R.NextInt(2); break;
	case EMCBiome::WarmOcean: case EMCBiome::LukewarmOcean: Mob = R.Chance(0.5) ? FName(TEXT("tropical_fish")) : FName(TEXT("dolphin")); break;
	case EMCBiome::Ocean: case EMCBiome::DeepOcean: case EMCBiome::ColdOcean: case EMCBiome::DeepColdOcean: Mob = R.Chance(0.5) ? FName(TEXT("cod")) : FName(TEXT("squid")); break;
	case EMCBiome::River: Mob = TEXT("salmon"); break;
	default: return;
	}
	const bool bWaterMob = Mob == TEXT("cod") || Mob == TEXT("squid") || Mob == TEXT("salmon") || Mob == TEXT("tropical_fish") || Mob == TEXT("dolphin");
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 ax = FMath::Clamp(lx + R.Range(-3, 3), 0, 15), ay = FMath::Clamp(ly + R.Range(-3, 3), 0, 15);
		const int32 AZ = P.Surface[ax + ay * 16];
		const FMCState Above = P.Get(ax, ay, AZ + 1);
		if (bWaterMob != (Above == FMCGenBlocks::Get().Water)) continue;
		const double SpawnZ = bWaterMob ? FMath::Min<double>(AZ + 2, P.WaterTop[ax + ay * 16] - 0.5) : AZ + 1.0;
		W.AddSpawn(Mob, W.X0 + ax + 0.5, W.Y0 + ay + 0.5, SpawnZ);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Generate

void FMCOverworldGen::Generate(FMCChunk& C) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	TSharedPtr<const FMCProtoChunk> P = GetProto(C.Pos.X, C.Pos.Y);

	// 1) copy terrain
	for (int32 Z = MC::MinZ; Z <= MC::MaxZ; ++Z)
	{
		const uint16* Src = P->Blocks.GetData() + (Z - MC::MinZ) * 256;
		bool bAny = false;
		for (int32 i = 0; i < 256; ++i) if (Src[i]) { bAny = true; break; }
		if (!bAny) continue;
		FMCSection* S = C.GetOrCreateSection(FMCChunk::SectionOf(Z));
		FMemory::Memcpy(&S->States[((Z - MC::MinZ) & 15) * 256], Src, 256 * sizeof(uint16));
	}

	// 2) biomes (surface + cave)
	for (int32 cy = 0; cy < 4; ++cy)
	{
		for (int32 cx = 0; cx < 4; ++cx)
		{
			const int32 lx = cx * 4 + 2, ly = cy * 4 + 2;
			const uint8 Surf = P->Biome[lx + ly * 16];
			const int32 SurfZ = P->Surface[lx + ly * 16];
			const FMCClimate Cl = SampleClimate(C.Pos.MinBlockX() + lx, C.Pos.MinBlockY() + ly);
			for (int32 cz = 0; cz < MC::WorldHeight / 4; ++cz)
			{
				const int32 Z = MC::MinZ + cz * 4 + 2;
				uint8 Bi = Surf;
				if (Z < SurfZ - 12)
				{
					const uint8 Cave = PickCaveBiome(C.Pos.MinBlockX() + lx, C.Pos.MinBlockY() + ly, Z, Cl);
					if (Cave != 255) Bi = Cave;
				}
				C.SetBiomeCell(cx, cy, cz, Bi);
			}
		}
	}

	FMCGenWriter W(C);
	// 3) ores from the 3x3 neighbourhood (blobs may cross chunk borders)
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			TSharedPtr<const FMCProtoChunk> NP = (dx == 0 && dy == 0) ? P : GetProto(C.Pos.X + dx, C.Pos.Y + dy);
			PlaceOres(W, *NP, C.Pos.X + dx, C.Pos.Y + dy);
		}
	// 4) cave decoration
	PlaceCaveDecor(W, *P);
	// 5) surface features from the 3x3 neighbourhood
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
			PlaceFeatures(W, C.Pos.X + dx, C.Pos.Y + dy);
	// 6) structures
	PlaceStructures(W, C);
	// 7) snow cover on cold columns
	for (int32 ly = 0; ly < 16; ++ly)
	{
		for (int32 lx = 0; lx < 16; ++lx)
		{
			const uint8 Bi = P->Biome[lx + ly * 16];
			const FMCBiomeDef& BD = FMCBiomes::Get(Bi);
			int32 Top = MC::MaxZ;
			while (Top > MC::MinZ && C.Get(lx, ly, Top) == 0) --Top;
			const bool bHighSnow = Top > 160 + (int32)(P->Temp[lx + ly * 16] * 40.f);
			if (!BD.bSnowy && !bHighSnow) continue;
			const FMCState S = C.Get(lx, ly, Top);
			const FMCStateInfo& I = FMCBlocks::Info(S);
			if (S == G.Water) { C.SetRaw(lx, ly, Top, G.Ice); continue; }
			if ((I.Flags & (MCB_Opaque | MCB_Leaves)) && Top < MC::MaxZ)
			{
				C.SetRaw(lx, ly, Top + 1, G.Snow);
				if (S == G.Grass) C.SetRaw(lx, ly, Top, G.GrassSnowy);
			}
			else if ((I.Flags & MCB_Plant) && S != G.SugarCane && S != G.Cactus && Top > MC::MinZ)
			{
				const FMCState Under = C.Get(lx, ly, Top - 1);
				if (FMCBlocks::IsOpaque(Under)) C.SetRaw(lx, ly, Top, G.Snow);
			}
		}
	}
	// 8) animals
	SpawnInitialAnimals(W, *P);
	C.bPopulated = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

uint8 FMCOverworldGen::GetBiomeAt(int32 X, int32 Y, int32 Z) const
{
	const FMCClimate Cl = SampleClimate(X, Y);
	const int32 SurfZ = (int32)Cl.BaseHeight;
	if (Z < SurfZ - 12)
	{
		const uint8 Cave = PickCaveBiome(X, Y, Z, Cl);
		if (Cave != 255) return Cave;
	}
	return PickSurfaceBiome(Cl, SurfZ);
}

int32 FMCOverworldGen::GetSurfaceHeight(int32 X, int32 Y) const
{
	return (int32)SampleClimate(X, Y).BaseHeight;
}

int32 FMCOverworldGen::SampleSurfaceEstimate(int32 X, int32 Y) const
{
	return (int32)SampleClimate(X, Y).BaseHeight;
}

FMCBlockPos FMCOverworldGen::FindSpawn() const
{
	// Prefer a spot next to a village in a grassy biome (great first impression), else any dry grassland.
	const FMCStructureSet* VSet = StructureSets.FindByPredicate([](const FMCStructureSet& S) { return S.Type == TEXT("village"); });
	if (VSet)
	{
		for (int32 R = 0; R <= 3; ++R)
			for (int32 ry = -R; ry <= R; ++ry)
				for (int32 rx = -R; rx <= R; ++rx)
				{
					if (FMath::Max(FMath::Abs(rx), FMath::Abs(ry)) != R) continue;
					FMCChunkPos SC;
					MCGen::GetStructureStartChunk(*VSet, Seed, rx, ry, SC);
					TSharedPtr<const FMCStructureStart> St = GetStructureStart(TEXT("village"), SC);
					if (St.IsValid() && St->bValid)
					{
						const int32 X = St->Origin.X - 14, Y = St->Origin.Y - 14;
						return FMCBlockPos(X, Y, ProtoSurface(X, Y) + 1);
					}
				}
	}
	for (int32 Rad = 0; Rad < 4000; Rad += 48)
	{
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = i / 16.f * 2.f * PI;
			const int32 X = (int32)(FMath::Cos(A) * Rad), Y = (int32)(FMath::Sin(A) * Rad);
			const FMCClimate Cl = SampleClimate(X, Y);
			const EMCBiome E = (EMCBiome)Cl.Biome;
			if (Cl.BaseHeight > MC::SeaLevel + 1 && Cl.BaseHeight < 110 && (E == EMCBiome::Plains || E == EMCBiome::Forest || E == EMCBiome::Meadow || E == EMCBiome::FlowerForest || E == EMCBiome::SunflowerPlains || E == EMCBiome::BirchForest || E == EMCBiome::CherryGrove || E == EMCBiome::Savanna))
			{
				return FMCBlockPos(X, Y, ProtoSurface(X, Y) + 1);
			}
		}
	}
	return FMCBlockPos(0, 0, ProtoSurface(0, 0) + 1);
}
