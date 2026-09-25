// Small DSL used by the recipe definition files.
#pragma once

#include "CoreMinimal.h"
#include "Crafting/MCRecipes.h"
#include <initializer_list>

class FMCRecipeBuilder
{
public:
	int32 Skipped = 0;
	using K = TPair<TCHAR, const TCHAR*>;
	bool Shaped(const TCHAR* Result, int32 Count, std::initializer_list<const TCHAR*> Rows, std::initializer_list<TPair<TCHAR, const TCHAR*>> Keys, EMCRecipeBook Book = EMCRecipeBook::Misc);
	bool Shapeless(const TCHAR* Result, int32 Count, std::initializer_list<const TCHAR*> Ingredients, EMCRecipeBook Book = EMCRecipeBook::Misc);
	bool Smelt(const TCHAR* Input, const TCHAR* Output, float XP = 0.1f, int32 Time = 200, uint8 Stations = MCSS_Furnace, int32 Count = 1);
	bool Stonecut(const TCHAR* Input, const TCHAR* Output, int32 Count = 1);
	bool Brew(const TCHAR* Ingredient, const TCHAR* FromPotion, const TCHAR* ToPotion);
	bool Smith(const TCHAR* Template, const TCHAR* Base, const TCHAR* Addition, const TCHAR* Result);
	void Compost(const TCHAR* Item, int32 Chance);
	static bool Exists(const TCHAR* Item) { return FMCItems::FindId(FName(Item)) != 0; }
};
