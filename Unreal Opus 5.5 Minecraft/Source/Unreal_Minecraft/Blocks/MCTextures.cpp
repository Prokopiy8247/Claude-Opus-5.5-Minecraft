#include "Blocks/MCTextures.h"
#include "Core/MCCore.h"

namespace
{
	TArray<FMCTexDef> GTexDefs;
	TMap<FName, int16> GTexIndex;
	bool GTexInit = false;
}

// Registration groups (implemented in MCTextureDefs*.cpp)
void MCRegisterTerrainTextures(TFunctionRef<void(const FMCTexDef&)> Add);
void MCRegisterWoodTextures(TFunctionRef<void(const FMCTexDef&)> Add);
void MCRegisterMineralTextures(TFunctionRef<void(const FMCTexDef&)> Add);
void MCRegisterPlantTextures(TFunctionRef<void(const FMCTexDef&)> Add);
void MCRegisterFunctionalTextures(TFunctionRef<void(const FMCTexDef&)> Add);
void MCRegisterNetherEndTextures(TFunctionRef<void(const FMCTexDef&)> Add);

void FMCTextures::Init()
{
	if (GTexInit) return;
	GTexInit = true;
	GTexDefs.Reset();
	GTexIndex.Reset();
	RegisterAll();
	UE_LOG(LogOpus55, Log, TEXT("Registered %d block texture layers"), GTexDefs.Num());
}

void FMCTextures::Add(const FMCTexDef& Def)
{
	if (GTexIndex.Contains(Def.Name))
	{
		return; // first definition wins (keeps layer order stable)
	}
	FMCTexDef D = Def;
	if (D.Seed == 0) D.Seed = (uint32)MCHash::StringHash(*Def.Name.ToString());
	const int16 Index = (int16)GTexDefs.Add(D);
	GTexIndex.Add(Def.Name, Index);
}

void FMCTextures::RegisterAll()
{
	auto AddFn = [](const FMCTexDef& D) { FMCTextures::Add(D); };
	// Layer 0 is the neutral fallback texture.
	{
		FMCTexDef D; D.Name = TEXT("missing"); D.Recipe = EMCTexRecipe::Noise; D.C0 = FColor(120, 110, 120); D.C1 = FColor(150, 140, 150);
		Add(D);
	}
	MCRegisterTerrainTextures(AddFn);
	MCRegisterWoodTextures(AddFn);
	MCRegisterMineralTextures(AddFn);
	MCRegisterPlantTextures(AddFn);
	MCRegisterFunctionalTextures(AddFn);
	MCRegisterNetherEndTextures(AddFn);
}

const TArray<FMCTexDef>& FMCTextures::Defs()
{
	Init();
	return GTexDefs;
}

int16 FMCTextures::Find(FName Name)
{
	Init();
	if (const int16* I = GTexIndex.Find(Name)) return *I;
	return 0;
}

int16 FMCTextures::FindChecked(const TCHAR* Name)
{
	Init();
	if (const int16* I = GTexIndex.Find(FName(Name))) return *I;
	UE_LOG(LogOpus55, Warning, TEXT("Missing texture definition '%s' (using fallback)"), Name);
	return 0;
}

bool FMCTextures::Has(FName Name)
{
	Init();
	return GTexIndex.Contains(Name);
}

int32 FMCTextures::Num()
{
	Init();
	return GTexDefs.Num();
}

const FMCTexDef* FMCTextures::FindDef(FName Name)
{
	Init();
	if (const int16* I = GTexIndex.Find(Name)) return &GTexDefs[*I];
	return nullptr;
}

FColor FMCTextures::AverageColor(int16 Layer)
{
	Init();
	if (!GTexDefs.IsValidIndex(Layer)) return FColor(128, 128, 128);
	const FMCTexDef& D = GTexDefs[Layer];
	return FColor((D.C0.R + D.C1.R) / 2, (D.C0.G + D.C1.G) / 2, (D.C0.B + D.C1.B) / 2, 255);
}
