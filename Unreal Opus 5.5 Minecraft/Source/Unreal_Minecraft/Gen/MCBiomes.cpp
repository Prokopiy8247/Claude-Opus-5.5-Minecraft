#include "Gen/MCBiomes.h"

namespace
{
	FMCBiomeDef GBiomes[(int32)EMCBiome::Count];
	bool GBiomesInit = false;

	FColor Hx(uint32 H) { return FColor((H >> 16) & 255, (H >> 8) & 255, H & 255, 255); }

	void Def(EMCBiome Id, const TCHAR* Name, float Temp, float Down, uint32 Grass, uint32 Foliage, uint32 Water, uint32 Sky = 0x78A7FF)
	{
		FMCBiomeDef& B = GBiomes[(int32)Id];
		B.Id = Id;
		B.Name = FName(Name);
		FString S(Name);
		TArray<FString> Parts; S.ParseIntoArray(Parts, TEXT("_"));
		for (FString& P : Parts) { if (P.Len()) P[0] = FChar::ToUpper(P[0]); B.Display += (B.Display.IsEmpty() ? P : TEXT(" ") + P); }
		B.Temperature = Temp;
		B.Downfall = Down;
		B.Grass = Hx(Grass);
		B.Foliage = Hx(Foliage);
		B.Water = Hx(Water);
		B.Sky = Hx(Sky);
		B.bSnowy = Temp < 0.15f;
		B.bDry = Down <= 0.0f;
		B.bFrozenWater = Temp < 0.15f;
	}
}

void FMCBiomes::Init()
{
	if (GBiomesInit) return;
	GBiomesInit = true;
	using B = EMCBiome;
	Def(B::Plains, TEXT("plains"), 0.8f, 0.4f, 0x91BD59, 0x77AB2F, 0x44AFF5, 0x78A7FF);
	Def(B::SunflowerPlains, TEXT("sunflower_plains"), 0.8f, 0.4f, 0x91BD59, 0x77AB2F, 0x44AFF5);
	Def(B::SnowyPlains, TEXT("snowy_plains"), 0.0f, 0.5f, 0x80B497, 0x60A17B, 0x3D57D6, 0x7FA1FF);
	Def(B::IceSpikes, TEXT("ice_spikes"), 0.0f, 0.5f, 0x80B497, 0x60A17B, 0x3D57D6, 0x7FA1FF);
	Def(B::Desert, TEXT("desert"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x32A598, 0x6EB1FF);
	Def(B::Swamp, TEXT("swamp"), 0.8f, 0.9f, 0x6A7039, 0x6A7039, 0x617B64, 0x78A7FF);
	Def(B::MangroveSwamp, TEXT("mangrove_swamp"), 0.8f, 0.9f, 0x6A7039, 0x8DB127, 0x3A7A6A, 0x78A7FF);
	Def(B::Forest, TEXT("forest"), 0.7f, 0.8f, 0x79C05A, 0x59AE30, 0x1E97F2, 0x79A6FF);
	Def(B::FlowerForest, TEXT("flower_forest"), 0.7f, 0.8f, 0x79C05A, 0x59AE30, 0x20A3CC);
	Def(B::BirchForest, TEXT("birch_forest"), 0.6f, 0.6f, 0x88BB67, 0x6BA941, 0x0677CE, 0x7AA5FF);
	Def(B::OldGrowthBirchForest, TEXT("old_growth_birch_forest"), 0.6f, 0.6f, 0x88BB67, 0x6BA941, 0x0A74C4);
	Def(B::DarkForest, TEXT("dark_forest"), 0.7f, 0.8f, 0x507A32, 0x59AE30, 0x3B6CD1, 0x79A6FF);
	Def(B::PaleGarden, TEXT("pale_garden"), 0.7f, 0.8f, 0x778272, 0x878D76, 0x76889D, 0xB9B9B9);
	Def(B::Taiga, TEXT("taiga"), 0.25f, 0.8f, 0x86B783, 0x68A464, 0x287082, 0x7DA3FF);
	Def(B::OldGrowthPineTaiga, TEXT("old_growth_pine_taiga"), 0.3f, 0.8f, 0x86B87F, 0x68A55F, 0x2D6D77);
	Def(B::OldGrowthSpruceTaiga, TEXT("old_growth_spruce_taiga"), 0.25f, 0.8f, 0x86B87F, 0x68A55F, 0x2D6D77);
	Def(B::SnowyTaiga, TEXT("snowy_taiga"), -0.5f, 0.4f, 0x80B497, 0x60A17B, 0x205E83, 0x839EFF);
	Def(B::Savanna, TEXT("savanna"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x2C8B9C, 0x6EB1FF);
	Def(B::SavannaPlateau, TEXT("savanna_plateau"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x2590A8, 0x6EB1FF);
	Def(B::WindsweptSavanna, TEXT("windswept_savanna"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x2590A8, 0x6EB1FF);
	Def(B::WindsweptHills, TEXT("windswept_hills"), 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x007BF7, 0x7DA2FF);
	Def(B::WindsweptGravellyHills, TEXT("windswept_gravelly_hills"), 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x0E63AB, 0x7DA2FF);
	Def(B::WindsweptForest, TEXT("windswept_forest"), 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x0E63AB, 0x7DA2FF);
	Def(B::Jungle, TEXT("jungle"), 0.95f, 0.9f, 0x59C93C, 0x30BB0B, 0x14A2C5, 0x77A8FF);
	Def(B::SparseJungle, TEXT("sparse_jungle"), 0.95f, 0.8f, 0x64C73F, 0x3EB80F, 0x0D8AE3, 0x77A8FF);
	Def(B::BambooJungle, TEXT("bamboo_jungle"), 0.95f, 0.9f, 0x59C93C, 0x30BB0B, 0x14A2C5, 0x77A8FF);
	Def(B::Badlands, TEXT("badlands"), 2.0f, 0.0f, 0x90814D, 0x9E814D, 0x4E7F81, 0x6EB1FF);
	Def(B::ErodedBadlands, TEXT("eroded_badlands"), 2.0f, 0.0f, 0x90814D, 0x9E814D, 0x497F99, 0x6EB1FF);
	Def(B::WoodedBadlands, TEXT("wooded_badlands"), 2.0f, 0.0f, 0x90814D, 0x9E814D, 0x55809E, 0x6EB1FF);
	Def(B::Meadow, TEXT("meadow"), 0.5f, 0.8f, 0x83BB6D, 0x63A948, 0x0E4ECF, 0x7BA4FF);
	Def(B::CherryGrove, TEXT("cherry_grove"), 0.5f, 0.8f, 0xB6DB61, 0xB6DB61, 0x5DB7EF, 0x7BA4FF);
	Def(B::Grove, TEXT("grove"), -0.2f, 0.8f, 0x80B497, 0x60A17B, 0x3D57D6, 0x81A0FF);
	Def(B::SnowySlopes, TEXT("snowy_slopes"), -0.3f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6, 0x829FFF);
	Def(B::FrozenPeaks, TEXT("frozen_peaks"), -0.7f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6, 0x859DFF);
	Def(B::JaggedPeaks, TEXT("jagged_peaks"), -0.7f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6, 0x859DFF);
	Def(B::StonyPeaks, TEXT("stony_peaks"), 1.0f, 0.3f, 0x9ABE4B, 0x82AC1E, 0x0D67BB, 0x76A8FF);
	Def(B::River, TEXT("river"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x0084FF, 0x7BA4FF);
	Def(B::FrozenRiver, TEXT("frozen_river"), 0.0f, 0.5f, 0x80B497, 0x60A17B, 0x185390, 0x7FA1FF);
	Def(B::Beach, TEXT("beach"), 0.8f, 0.4f, 0x91BD59, 0x77AB2F, 0x157CAB, 0x78A7FF);
	Def(B::SnowyBeach, TEXT("snowy_beach"), 0.05f, 0.3f, 0x80B497, 0x60A17B, 0x1463A5, 0x7FA1FF);
	Def(B::StonyShore, TEXT("stony_shore"), 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x0D67BB, 0x7DA2FF);
	Def(B::WarmOcean, TEXT("warm_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x43D5EE, 0x7BA4FF);
	Def(B::LukewarmOcean, TEXT("lukewarm_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x45ADF2, 0x7BA4FF);
	Def(B::DeepLukewarmOcean, TEXT("deep_lukewarm_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x45ADF2, 0x7BA4FF);
	Def(B::Ocean, TEXT("ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x7BA4FF);
	Def(B::DeepOcean, TEXT("deep_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x7BA4FF);
	Def(B::ColdOcean, TEXT("cold_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3D57D6, 0x7BA4FF);
	Def(B::DeepColdOcean, TEXT("deep_cold_ocean"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3D57D6, 0x7BA4FF);
	Def(B::FrozenOcean, TEXT("frozen_ocean"), 0.0f, 0.5f, 0x80B497, 0x60A17B, 0x3938C9, 0x7FA1FF);
	Def(B::DeepFrozenOcean, TEXT("deep_frozen_ocean"), 0.5f, 0.5f, 0x80B497, 0x60A17B, 0x3938C9, 0x7BA4FF);
	Def(B::MushroomFields, TEXT("mushroom_fields"), 0.9f, 1.0f, 0x55C93F, 0x2BBB0F, 0x8A8997, 0x77A8FF);
	Def(B::DripstoneCaves, TEXT("dripstone_caves"), 0.8f, 0.4f, 0x91BD59, 0x77AB2F, 0x3F76E4);
	Def(B::LushCaves, TEXT("lush_caves"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4);
	Def(B::DeepDark, TEXT("deep_dark"), 0.8f, 0.4f, 0x91BD59, 0x77AB2F, 0x3F76E4);
	Def(B::SulfurCaves, TEXT("sulfur_caves"), 1.2f, 0.2f, 0x9AA84A, 0x8A9A2A, 0x6E9A3A);
	Def(B::NetherWastes, TEXT("nether_wastes"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x330808);
	Def(B::CrimsonForest, TEXT("crimson_forest"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x330303);
	Def(B::WarpedForest, TEXT("warped_forest"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x1A051A);
	Def(B::SoulSandValley, TEXT("soul_sand_valley"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x1B4745);
	Def(B::BasaltDeltas, TEXT("basalt_deltas"), 2.0f, 0.0f, 0xBFB755, 0xAEA42A, 0x3F76E4, 0x685F70);
	Def(B::TheEnd, TEXT("the_end"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x000000);
	Def(B::EndHighlands, TEXT("end_highlands"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x000000);
	Def(B::EndMidlands, TEXT("end_midlands"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x000000);
	Def(B::SmallEndIslands, TEXT("small_end_islands"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x000000);
	Def(B::EndBarrens, TEXT("end_barrens"), 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3F76E4, 0x000000);

	for (int32 i = (int32)B::WarmOcean; i <= (int32)B::DeepFrozenOcean; ++i) GBiomes[i].bOcean = true;
	GBiomes[(int32)B::DeepFrozenOcean].bFrozenWater = true;
	GBiomes[(int32)B::FrozenOcean].bFrozenWater = true;
	for (int32 i = (int32)B::DripstoneCaves; i <= (int32)B::SulfurCaves; ++i) GBiomes[i].bCave = true;
	for (int32 i = (int32)B::NetherWastes; i <= (int32)B::BasaltDeltas; ++i) { GBiomes[i].bNether = true; GBiomes[i].bDry = true; GBiomes[i].bSnowy = false; }
	for (int32 i = (int32)B::TheEnd; i <= (int32)B::EndBarrens; ++i) { GBiomes[i].bEnd = true; GBiomes[i].bDry = true; }
	GBiomes[(int32)B::Desert].bDry = true;
	GBiomes[(int32)B::Savanna].bDry = true;
	GBiomes[(int32)B::Badlands].bDry = true;
	GBiomes[(int32)B::ErodedBadlands].bDry = true;
	GBiomes[(int32)B::WoodedBadlands].bDry = true;
	GBiomes[(int32)B::NetherWastes].Fog = Hx(0x330808);
	GBiomes[(int32)B::CrimsonForest].Fog = Hx(0x330303);
	GBiomes[(int32)B::WarpedForest].Fog = Hx(0x1A051A);
	GBiomes[(int32)B::SoulSandValley].Fog = Hx(0x1B4745);
	GBiomes[(int32)B::BasaltDeltas].Fog = Hx(0x685F70);
	for (int32 i = (int32)B::TheEnd; i <= (int32)B::EndBarrens; ++i) GBiomes[i].Fog = Hx(0x0A080C);
}

const FMCBiomeDef& FMCBiomes::Get(uint8 Id)
{
	Init();
	return GBiomes[FMath::Min<int32>(Id, (int32)EMCBiome::Count - 1)];
}

EMCBiome FMCBiomes::FromName(const FString& Name)
{
	Init();
	for (int32 i = 0; i < (int32)EMCBiome::Count; ++i)
	{
		if (GBiomes[i].Name.ToString().Equals(Name, ESearchCase::IgnoreCase)) return (EMCBiome)i;
	}
	return EMCBiome::Count;
}
