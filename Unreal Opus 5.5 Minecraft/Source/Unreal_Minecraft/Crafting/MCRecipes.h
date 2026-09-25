// Crafting, smelting, stonecutting, brewing and smithing recipe registry.
#pragma once

#include "CoreMinimal.h"
#include "Items/MCItems.h"

struct UNREAL_MINECRAFT_API FMCIngredient
{
	TArray<FMCItemId> Items;
	bool IsEmpty() const { return Items.Num() == 0; }
	bool Matches(const FMCItemStack& S) const
	{
		if (Items.Num() == 0) return S.IsEmpty();
		if (S.IsEmpty()) return false;
		return Items.Contains(S.Id);
	}
	FMCItemId First() const { return Items.Num() ? Items[0] : 0; }
};

enum class EMCRecipeBook : uint8 { Building, Redstone, Equipment, Misc, Food };

struct UNREAL_MINECRAFT_API FMCRecipe
{
	FName Id;
	bool bShaped = true;
	int32 W = 0, H = 0;
	TArray<FMCIngredient> Grid;   // shaped: W*H (row major, top row first); shapeless: list
	FMCItemStack Result;
	EMCRecipeBook Book = EMCRecipeBook::Misc;
	bool bSpecial = false;        // dynamic result (repair, dye mixing, firework...)
};

enum EMCSmeltStation : uint8
{
	MCSS_Furnace = 1, MCSS_Blast = 2, MCSS_Smoker = 4, MCSS_Campfire = 8
};

struct FMCSmeltRecipe
{
	FMCIngredient Input;
	FMCItemId Output = 0;
	int32 Count = 1;
	float XP = 0.1f;
	int32 CookTime = 200;
	uint8 Stations = MCSS_Furnace;
};

struct FMCStonecutRecipe
{
	FMCItemId Input = 0;
	FMCItemId Output = 0;
	int32 Count = 1;
};

struct FMCBrewRecipe
{
	FMCItemId Ingredient = 0;
	int32 FromPotion = 0;
	int32 ToPotion = 0;
};

struct FMCSmithRecipe
{
	FMCItemId Template = 0;
	FMCItemId Base = 0;
	FMCItemId Addition = 0;
	FMCItemId Result = 0;
};

class UNREAL_MINECRAFT_API FMCRecipes
{
public:
	static void Init();
	/** Match a crafting grid of W x H stacks (row major). Returns nullptr if no recipe matches. */
	static const FMCRecipe* FindCrafting(const FMCItemStack* Grid, int32 W, int32 H, FMCItemStack& OutResult);
	static const FMCSmeltRecipe* FindSmelting(const FMCItemStack& In, uint8 Station);
	static int32 FuelTicks(const FMCItemStack& S);
	static void GetStonecutting(FMCItemId Input, TArray<const FMCStonecutRecipe*>& Out);
	static int32 FindBrew(FMCItemId Ingredient, int32 FromPotion);
	static bool IsBrewIngredient(FMCItemId Ingredient);
	static const FMCSmithRecipe* FindSmithing(FMCItemId Template, FMCItemId Base, FMCItemId Addition);
	static bool IsCompostable(FMCItemId Id, int32& OutChance);

	static const TArray<FMCRecipe>& Crafting() { return CraftingRecipes; }
	static const TArray<FMCSmeltRecipe>& Smelting() { return SmeltRecipes; }
	/** All recipes producing this item (recipe book / tooltips). */
	static void FindRecipesFor(FMCItemId Result, TArray<const FMCRecipe*>& Out);
	/** Resolve "#tag" or item name to an ingredient. */
	static FMCIngredient Ing(const TCHAR* NameOrTag);

private:
	friend class FMCRecipeBuilder;
	static TArray<FMCRecipe> CraftingRecipes;
	static TArray<FMCSmeltRecipe> SmeltRecipes;
	static TArray<FMCStonecutRecipe> StonecutRecipes;
	static TArray<FMCBrewRecipe> BrewRecipes;
	static TArray<FMCSmithRecipe> SmithRecipes;
	static TMap<FMCItemId, int32> CompostChances;
	static bool bInitialized;
	static bool MatchShaped(const FMCRecipe& R, const FMCItemStack* Grid, int32 GW, int32 GH, int32 OX, int32 OY, bool bMirror);
};
