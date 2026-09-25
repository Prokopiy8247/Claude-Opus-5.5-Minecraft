// Recipe registry: matching for crafting grids (shaped/mirrored/shapeless + special recipes), smelting,
// stonecutting, brewing, smithing and composting.
#include "Crafting/MCRecipes.h"
#include "Crafting/MCRecipeBuilder.h"

TArray<FMCRecipe> FMCRecipes::CraftingRecipes;
TArray<FMCSmeltRecipe> FMCRecipes::SmeltRecipes;
TArray<FMCStonecutRecipe> FMCRecipes::StonecutRecipes;
TArray<FMCBrewRecipe> FMCRecipes::BrewRecipes;
TArray<FMCSmithRecipe> FMCRecipes::SmithRecipes;
TMap<FMCItemId, int32> FMCRecipes::CompostChances;
bool FMCRecipes::bInitialized = false;

void MCRegisterCraftingRecipes(FMCRecipeBuilder& B);
void MCRegisterProcessingRecipes(FMCRecipeBuilder& B);

namespace
{
	TMap<FMCItemId, TArray<int32>> GResultIndex;
}

FMCIngredient FMCRecipes::Ing(const TCHAR* NameOrTag)
{
	FMCIngredient I;
	if (!NameOrTag || !*NameOrTag) return I;
	if (NameOrTag[0] == TEXT('#'))
	{
		const FName Tag(NameOrTag + 1);
		if (Tag == TEXT("stone_tool_materials"))
		{
			for (const TCHAR* N : { TEXT("cobblestone"), TEXT("blackstone"), TEXT("cobbled_deepslate") }) if (const FMCItemId Id = FMCItems::FindId(N)) I.Items.Add(Id);
			return I;
		}
		if (Tag == TEXT("coals"))
		{
			for (const TCHAR* N : { TEXT("coal"), TEXT("charcoal") }) if (const FMCItemId Id = FMCItems::FindId(N)) I.Items.Add(Id);
			return I;
		}
		if (Tag == TEXT("soul_fire_base_blocks"))
		{
			for (const TCHAR* N : { TEXT("soul_sand"), TEXT("soul_soil") }) if (const FMCItemId Id = FMCItems::FindId(N)) I.Items.Add(Id);
			return I;
		}
		if (Tag == TEXT("stone_crafting_materials"))
		{
			for (const TCHAR* N : { TEXT("cobblestone"), TEXT("blackstone"), TEXT("cobbled_deepslate") }) if (const FMCItemId Id = FMCItems::FindId(N)) I.Items.Add(Id);
			return I;
		}
		for (const FMCItem& It : FMCItems::All())
		{
			if (It.Id == 0 || It.bHidden) continue;
			if (It.Tags.Contains(Tag)) I.Items.Add(It.Id);
		}
		return I;
	}
	if (const FMCItemId Id = FMCItems::FindId(FName(NameOrTag))) I.Items.Add(Id);
	return I;
}

void FMCRecipes::Init()
{
	if (bInitialized) return;
	bInitialized = true;
	FMCRecipeBuilder B;
	MCRegisterCraftingRecipes(B);
	MCRegisterProcessingRecipes(B);
	GResultIndex.Reset();
	for (int32 i = 0; i < CraftingRecipes.Num(); ++i) GResultIndex.FindOrAdd(CraftingRecipes[i].Result.Id).Add(i);
	UE_LOG(LogOpus55, Log, TEXT("Recipes: %d crafting, %d smelting, %d stonecutting, %d brewing, %d smithing (%d skipped)"),
		CraftingRecipes.Num(), SmeltRecipes.Num(), StonecutRecipes.Num(), BrewRecipes.Num(), SmithRecipes.Num(), B.Skipped);
}

bool FMCRecipes::MatchShaped(const FMCRecipe& R, const FMCItemStack* Grid, int32 GW, int32 GH, int32 OX, int32 OY, bool bMirror)
{
	for (int32 y = 0; y < GH; ++y)
		for (int32 x = 0; x < GW; ++x)
		{
			const int32 RX = x - OX, RY = y - OY;
			const FMCItemStack& S = Grid[x + y * GW];
			if (RX < 0 || RY < 0 || RX >= R.W || RY >= R.H)
			{
				if (!S.IsEmpty()) return false;
				continue;
			}
			const int32 SrcX = bMirror ? (R.W - 1 - RX) : RX;
			const FMCIngredient& Ing = R.Grid[SrcX + RY * R.W];
			if (!Ing.Matches(S)) return false;
		}
	return true;
}

const FMCRecipe* FMCRecipes::FindCrafting(const FMCItemStack* Grid, int32 GW, int32 GH, FMCItemStack& OutResult)
{
	Init();
	OutResult.Clear();
	int32 NumItems = 0;
	for (int32 i = 0; i < GW * GH; ++i) if (!Grid[i].IsEmpty()) ++NumItems;
	if (NumItems == 0) return nullptr;

	// ---- special recipes (repair, fireworks, dyeing, tipped arrows)
	{
		TArray<const FMCItemStack*> Present;
		for (int32 i = 0; i < GW * GH; ++i) if (!Grid[i].IsEmpty()) Present.Add(&Grid[i]);
		// tool repair: two damageable items of the same kind
		if (Present.Num() == 2 && Present[0]->Id == Present[1]->Id && Present[0]->IsDamageable() && Present[0]->Count == 1 && Present[1]->Count == 1)
		{
			const int32 Max = Present[0]->Item().MaxDamage;
			const int32 Rem = (Max - Present[0]->Damage) + (Max - Present[1]->Damage) + Max * 5 / 100;
			OutResult = FMCItemStack(Present[0]->Id, 1, FMath::Max(0, Max - Rem));
			static FMCRecipe Repair; Repair.Id = TEXT("repair_item"); Repair.bSpecial = true; Repair.bShaped = false;
			return &Repair;
		}
		// firework rocket: paper + 1-3 gunpowder (+ stars)
		int32 Paper = 0, Gunpowder = 0, Other = 0;
		for (const FMCItemStack* S : Present)
		{
			const FName N = S->Item().Name;
			if (N == TEXT("paper")) ++Paper; else if (N == TEXT("gunpowder")) ++Gunpowder; else if (N == TEXT("firework_star")) {} else ++Other;
		}
		if (Paper == 1 && Gunpowder >= 1 && Gunpowder <= 3 && Other == 0)
		{
			OutResult = FMCItemStack::Of(TEXT("firework_rocket"), 3);
			OutResult.MutableExtra().FlightDuration = (uint8)Gunpowder;
			static FMCRecipe Fw; Fw.Id = TEXT("firework_rocket"); Fw.bSpecial = true; Fw.bShaped = false;
			return &Fw;
		}
		// leather armour dyeing: one leather piece + dyes
		const FMCItemStack* Armor = nullptr;
		TArray<uint8> Dyes;
		bool bInvalid = false;
		for (const FMCItemStack* S : Present)
		{
			const FMCItem& I = S->Item();
			if (I.ArmorMaterial == TEXT("leather") || I.Name == TEXT("leather_horse_armor")) { if (Armor) bInvalid = true; Armor = S; }
			else if (I.Kind == EMCItemKind::Dye) Dyes.Add(I.DyeColor);
			else bInvalid = true;
		}
		if (Armor && Dyes.Num() > 0 && !bInvalid)
		{
			OutResult = Armor->Copy();
			OutResult.MutableExtra().Color = Dyes.Last();
			static FMCRecipe Dye; Dye.Id = TEXT("armor_dye"); Dye.bSpecial = true; Dye.bShaped = false;
			return &Dye;
		}
		// tipped arrows: 8 arrows around a lingering potion (3x3 only)
		if (GW == 3 && GH == 3 && NumItems == 9 && Grid[4].Item().Name == TEXT("lingering_potion"))
		{
			bool bArrows = true;
			for (int32 i = 0; i < 9; ++i) if (i != 4 && Grid[i].Item().Name != TEXT("arrow")) bArrows = false;
			if (bArrows)
			{
				OutResult = FMCItemStack::Of(TEXT("tipped_arrow"), 8);
				OutResult.MutableExtra().Potion = Grid[4].Extra.IsValid() ? Grid[4].Extra->Potion : 0;
				static FMCRecipe Tip; Tip.Id = TEXT("tipped_arrow"); Tip.bSpecial = true;
				return &Tip;
			}
		}
		// map cloning: filled map + empty maps
		int32 Filled = 0, Empty = 0;
		for (const FMCItemStack* S : Present) { const FName N = S->Item().Name; if (N == TEXT("filled_map")) ++Filled; else if (N == TEXT("map")) ++Empty; }
		if (Filled == 1 && Empty >= 1 && Filled + Empty == Present.Num())
		{
			for (const FMCItemStack* S : Present) if (S->Item().Name == TEXT("filled_map")) { OutResult = S->Copy(); OutResult.Count = Empty + 1; }
			static FMCRecipe Clone; Clone.Id = TEXT("map_cloning"); Clone.bSpecial = true;
			return &Clone;
		}
	}

	// ---- bounds of the used area
	int32 MinX = GW, MinY = GH, MaxX = -1, MaxY = -1;
	for (int32 y = 0; y < GH; ++y)
		for (int32 x = 0; x < GW; ++x)
			if (!Grid[x + y * GW].IsEmpty()) { MinX = FMath::Min(MinX, x); MaxX = FMath::Max(MaxX, x); MinY = FMath::Min(MinY, y); MaxY = FMath::Max(MaxY, y); }
	const int32 UW = MaxX - MinX + 1, UH = MaxY - MinY + 1;

	for (const FMCRecipe& R : CraftingRecipes)
	{
		if (R.bShaped)
		{
			if (R.W != UW || R.H != UH) continue;
			if (MatchShaped(R, Grid, GW, GH, MinX, MinY, false) || MatchShaped(R, Grid, GW, GH, MinX, MinY, true))
			{
				OutResult = R.Result.Copy();
				return &R;
			}
		}
		else
		{
			if (R.Grid.Num() != NumItems) continue;
			TArray<bool> Used; Used.SetNumZeroed(R.Grid.Num());
			bool bOK = true;
			for (int32 i = 0; i < GW * GH && bOK; ++i)
			{
				const FMCItemStack& S = Grid[i];
				if (S.IsEmpty()) continue;
				bool bFound = false;
				for (int32 k = 0; k < R.Grid.Num(); ++k)
				{
					if (Used[k] || !R.Grid[k].Matches(S)) continue;
					Used[k] = true; bFound = true; break;
				}
				bOK = bFound;
			}
			if (bOK) { OutResult = R.Result.Copy(); return &R; }
		}
	}
	return nullptr;
}

const FMCSmeltRecipe* FMCRecipes::FindSmelting(const FMCItemStack& In, uint8 Station)
{
	Init();
	if (In.IsEmpty()) return nullptr;
	for (const FMCSmeltRecipe& R : SmeltRecipes)
		if ((R.Stations & Station) && R.Input.Matches(In)) return &R;
	return nullptr;
}

int32 FMCRecipes::FuelTicks(const FMCItemStack& S)
{
	if (S.IsEmpty()) return 0;
	const FMCItem& I = S.Item();
	if (I.FuelTicks > 0) return I.FuelTicks;
	if (I.Block) return FMCBlocks::Get(I.Block).FuelTicks;
	return 0;
}

void FMCRecipes::GetStonecutting(FMCItemId Input, TArray<const FMCStonecutRecipe*>& Out)
{
	Init();
	for (const FMCStonecutRecipe& R : StonecutRecipes) if (R.Input == Input) Out.Add(&R);
}

int32 FMCRecipes::FindBrew(FMCItemId Ingredient, int32 FromPotion)
{
	Init();
	for (const FMCBrewRecipe& R : BrewRecipes) if (R.Ingredient == Ingredient && R.FromPotion == FromPotion) return R.ToPotion;
	return -1;
}

bool FMCRecipes::IsBrewIngredient(FMCItemId Ingredient)
{
	Init();
	for (const FMCBrewRecipe& R : BrewRecipes) if (R.Ingredient == Ingredient) return true;
	const FName N = FMCItems::Get(Ingredient).Name;
	return N == TEXT("gunpowder") || N == TEXT("dragon_breath");
}

const FMCSmithRecipe* FMCRecipes::FindSmithing(FMCItemId Template, FMCItemId Base, FMCItemId Addition)
{
	Init();
	for (const FMCSmithRecipe& R : SmithRecipes) if (R.Template == Template && R.Base == Base && R.Addition == Addition) return &R;
	return nullptr;
}

bool FMCRecipes::IsCompostable(FMCItemId Id, int32& OutChance)
{
	Init();
	if (const int32* C = CompostChances.Find(Id)) { OutChance = *C; return true; }
	return false;
}

void FMCRecipes::FindRecipesFor(FMCItemId Result, TArray<const FMCRecipe*>& Out)
{
	Init();
	if (const TArray<int32>* Idx = GResultIndex.Find(Result)) for (int32 i : *Idx) Out.Add(&CraftingRecipes[i]);
}

// ---------------------------------------------------------------------------------------------------------------------
// Builder

bool FMCRecipeBuilder::Shaped(const TCHAR* Result, int32 Count, std::initializer_list<const TCHAR*> Rows, std::initializer_list<TPair<TCHAR, const TCHAR*>> Keys, EMCRecipeBook Book)
{
	const FMCItemId RId = FMCItems::FindId(FName(Result));
	if (!RId) { ++Skipped; return false; }
	FMCRecipe R;
	R.Id = FName(*FString::Printf(TEXT("%s_%d"), Result, FMCRecipes::CraftingRecipes.Num()));
	R.bShaped = true;
	R.H = (int32)Rows.size();
	R.W = 0;
	for (const TCHAR* Row : Rows) R.W = FMath::Max(R.W, (int32)FCString::Strlen(Row));
	R.Grid.SetNum(R.W * R.H);
	int32 y = 0;
	for (const TCHAR* Row : Rows)
	{
		const int32 Len = FCString::Strlen(Row);
		for (int32 x = 0; x < R.W; ++x)
		{
			const TCHAR C = x < Len ? Row[x] : TEXT(' ');
			if (C == TEXT(' ')) continue;
			const TCHAR* Key = nullptr;
			for (const TPair<TCHAR, const TCHAR*>& K : Keys) if (K.Key == C) Key = K.Value;
			if (!Key) { ++Skipped; return false; }
			FMCIngredient Ing = FMCRecipes::Ing(Key);
			if (Ing.IsEmpty()) { ++Skipped; return false; }
			R.Grid[x + y * R.W] = Ing;
		}
		++y;
	}
	R.Result = FMCItemStack(RId, Count);
	R.Book = Book;
	FMCRecipes::CraftingRecipes.Add(MoveTemp(R));
	return true;
}

bool FMCRecipeBuilder::Shapeless(const TCHAR* Result, int32 Count, std::initializer_list<const TCHAR*> Ingredients, EMCRecipeBook Book)
{
	const FMCItemId RId = FMCItems::FindId(FName(Result));
	if (!RId) { ++Skipped; return false; }
	FMCRecipe R;
	R.Id = FName(*FString::Printf(TEXT("%s_s%d"), Result, FMCRecipes::CraftingRecipes.Num()));
	R.bShaped = false;
	for (const TCHAR* I : Ingredients)
	{
		FMCIngredient Ing = FMCRecipes::Ing(I);
		if (Ing.IsEmpty()) { ++Skipped; return false; }
		R.Grid.Add(Ing);
	}
	R.Result = FMCItemStack(RId, Count);
	R.Book = Book;
	FMCRecipes::CraftingRecipes.Add(MoveTemp(R));
	return true;
}

bool FMCRecipeBuilder::Smelt(const TCHAR* Input, const TCHAR* Output, float XP, int32 Time, uint8 Stations, int32 Count)
{
	FMCIngredient In = FMCRecipes::Ing(Input);
	const FMCItemId Out = FMCItems::FindId(FName(Output));
	if (In.IsEmpty() || !Out) { ++Skipped; return false; }
	FMCSmeltRecipe R;
	R.Input = In; R.Output = Out; R.XP = XP; R.CookTime = Time; R.Stations = Stations; R.Count = Count;
	FMCRecipes::SmeltRecipes.Add(R);
	return true;
}

bool FMCRecipeBuilder::Stonecut(const TCHAR* Input, const TCHAR* Output, int32 Count)
{
	const FMCItemId In = FMCItems::FindId(FName(Input)), Out = FMCItems::FindId(FName(Output));
	if (!In || !Out) { ++Skipped; return false; }
	for (const FMCStonecutRecipe& E : FMCRecipes::StonecutRecipes) if (E.Input == In && E.Output == Out) return true;
	FMCStonecutRecipe R; R.Input = In; R.Output = Out; R.Count = Count;
	FMCRecipes::StonecutRecipes.Add(R);
	return true;
}

bool FMCRecipeBuilder::Brew(const TCHAR* Ingredient, const TCHAR* FromPotion, const TCHAR* ToPotion)
{
	const FMCItemId Ing = FMCItems::FindId(FName(Ingredient));
	const int32 From = MCPotions::Find(FromPotion), To = MCPotions::Find(ToPotion);
	if (!Ing || From < 0 || To < 0) { ++Skipped; return false; }
	FMCBrewRecipe R; R.Ingredient = Ing; R.FromPotion = From; R.ToPotion = To;
	FMCRecipes::BrewRecipes.Add(R);
	return true;
}

bool FMCRecipeBuilder::Smith(const TCHAR* Template, const TCHAR* Base, const TCHAR* Addition, const TCHAR* Result)
{
	const FMCItemId T = FMCItems::FindId(FName(Template)), B = FMCItems::FindId(FName(Base)), A = FMCItems::FindId(FName(Addition)), R = FMCItems::FindId(FName(Result));
	if (!T || !B || !A || !R) { ++Skipped; return false; }
	FMCSmithRecipe S; S.Template = T; S.Base = B; S.Addition = A; S.Result = R;
	FMCRecipes::SmithRecipes.Add(S);
	return true;
}

void FMCRecipeBuilder::Compost(const TCHAR* Item, int32 Chance)
{
	const FMCItemId Id = FMCItems::FindId(FName(Item));
	if (Id) FMCRecipes::CompostChances.Add(Id, Chance);
}
