// Workstation menus: enchanting, anvil, grindstone, stonecutter, smithing, beacon, loom, cartography, villager trading.
#include "Game/MCMenu.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCGame.h"
#include "Crafting/MCRecipes.h"
#include "World/MCWorld.h"

namespace
{
	int32 CountBookshelves(FMCWorld* W, const FMCBlockPos& P)
	{
		if (!W) return 0;
		int32 N = 0;
		for (int32 dz = 0; dz <= 1; ++dz)
			for (int32 dx = -2; dx <= 2; ++dx)
				for (int32 dy = -2; dy <= 2; ++dy)
				{
					if (FMath::Abs(dx) < 2 && FMath::Abs(dy) < 2) continue;
					const FMCBlockPos S(P.X + dx, P.Y + dy, P.Z + dz);
					if (FMCBlocks::GetByState(W->GetState(S)).Name != TEXT("bookshelf")) continue;
					// the block between the shelf and the table must be air
					const FMCBlockPos Mid(P.X + dx / 2, P.Y + dy / 2, P.Z + dz);
					if (W->GetState(Mid) != 0) continue;
					++N;
				}
		return FMath::Min(N, 15);
	}

	int32 EnchantCostWeight(EMCEnchant E, bool bBook)
	{
		const int32 W = MCEnchants::Info(E).Weight;
		int32 M = W >= 10 ? 1 : (W >= 5 ? 2 : (W >= 2 ? 4 : 8));
		if (bBook) M = FMath::Max(1, M / 2);
		return M;
	}

	bool IsLapis(const FMCItemStack& S) { return S.Item().Name == TEXT("lapis_lazuli"); }
}

// ---------------------------------------------------------------------------------------------------------------------
// Enchanting table

FMCEnchantMenu::FMCEnchantMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Enchanting, P)
{
	Title = TEXT("Enchant");
	Pos = InPos; bHasPos = true;
	Local.Init(2);
	const int32 ItemSlot = AddSlot(&Local, 0, 15, 47, MCSF_Single);
	(void)ItemSlot;
	const int32 L = AddSlot(&Local, 1, 35, 47, MCSF_Filtered);
	Slots[L].Filter = [](const FMCItemStack& S) { return IsLapis(S); };
	Slots[L].EmptyIcon = TEXT("empty_slot_lapis_lazuli");
	AddPlayerInventory(8, 84);
	Bookshelves = CountBookshelves(P->World, InPos);
}

bool FMCEnchantMenu::CanPlace(int32 SlotIndex, const FMCItemStack& S) const
{
	return FMCMenu::CanPlace(SlotIndex, S);
}

TArray<FMCEnchantLevel> FMCEnchantMenu::SelectEnchantments(FMCRandom& R, const FMCItemStack& Stack, int32 Level, bool bTreasure)
{
	TArray<FMCEnchantLevel> Out;
	if (Stack.IsEmpty()) return Out;
	const FMCItem& I = Stack.Item();
	const int32 Ench = FMath::Max(1, I.Enchantability > 0 ? I.Enchantability : (I.Name == TEXT("book") ? 1 : 0));
	if (I.Enchantability <= 0 && I.Name != TEXT("book")) return Out;
	int32 L = Level + 1 + R.NextInt(Ench / 4 + 1) + R.NextInt(Ench / 4 + 1);
	const float Bonus = (R.NextFloat() + R.NextFloat() - 1.f) * 0.15f;
	L = FMath::Clamp(FMath::RoundToInt(L + L * Bonus), 1, INT_MAX);

	auto Candidates = [&](int32 Lv)
	{
		TArray<FMCEnchantLevel> C;
		for (int32 e = 1; e < (int32)EMCEnchant::Count; ++e)
		{
			const EMCEnchant E = (EMCEnchant)e;
			const MCEnchants::FInfo& Info = MCEnchants::Info(E);
			if ((Info.bTreasure && !bTreasure) || Info.bCurse) continue;
			if (!MCEnchants::CanApply(E, I)) continue;
			for (int32 lv = Info.MaxLevel; lv >= 1; --lv)
			{
				if (Lv >= MCEnchants::MinCost(E, lv) && Lv <= MCEnchants::MaxCost(E, lv)) { FMCEnchantLevel X; X.Id = E; X.Level = (uint8)lv; C.Add(X); break; }
			}
		}
		return C;
	};
	auto PickWeighted = [&](TArray<FMCEnchantLevel>& C) -> int32
	{
		int32 Total = 0;
		for (const FMCEnchantLevel& X : C) Total += MCEnchants::Info(X.Id).Weight;
		if (Total <= 0) return -1;
		int32 Roll = R.NextInt(Total);
		for (int32 k = 0; k < C.Num(); ++k) { Roll -= MCEnchants::Info(C[k].Id).Weight; if (Roll < 0) return k; }
		return C.Num() - 1;
	};
	TArray<FMCEnchantLevel> C = Candidates(L);
	int32 K = PickWeighted(C);
	if (K < 0) return Out;
	Out.Add(C[K]);
	while (R.NextInt(50) <= L)
	{
		C.RemoveAll([&](const FMCEnchantLevel& X)
		{
			for (const FMCEnchantLevel& O : Out) if (!MCEnchants::Compatible(O.Id, X.Id)) return true;
			return false;
		});
		if (C.Num() == 0) break;
		K = PickWeighted(C);
		if (K < 0) break;
		Out.Add(C[K]);
		L /= 2;
	}
	return Out;
}

void FMCEnchantMenu::SlotsChanged()
{
	const FMCItemStack& S = Local[0];
	for (int32 i = 0; i < 3; ++i) { Costs[i] = 0; Hints[i] = EMCEnchant::None; HintLevels[i] = 0; }
	if (S.IsEmpty() || S.IsEnchanted()) return;
	const FMCItem& I = S.Item();
	if (I.Enchantability <= 0 && I.Name != TEXT("book")) return;
	FMCRandom R((uint64)Player->EnchantSeed);
	const int32 B = Bookshelves;
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 Base = R.Range(1, 8) + (B >> 1) + R.NextInt(B + 1);
		int32 Cost = i == 0 ? FMath::Max(Base / 3, 1) : (i == 1 ? Base * 2 / 3 + 1 : FMath::Max(Base, B * 2));
		if (Cost < i + 1) Cost = 0;
		Costs[i] = Cost;
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (Costs[i] <= 0) continue;
		FMCRandom HR((uint64)Player->EnchantSeed + i);
		const TArray<FMCEnchantLevel> L = SelectEnchantments(HR, S, Costs[i], false);
		if (L.Num() > 0)
		{
			const FMCEnchantLevel& Pick = L[HR.NextInt(L.Num())];
			Hints[i] = Pick.Id;
			HintLevels[i] = Pick.Level;
		}
	}
}

bool FMCEnchantMenu::ButtonClick(int32 Id)
{
	if (Id < 0 || Id > 2) return false;
	FMCItemStack& S = Local[0];
	FMCItemStack& Lap = Local[1];
	const int32 Cost = Costs[Id];
	if (S.IsEmpty() || Cost <= 0) return false;
	const bool bCreative = Player->IsCreative();
	if (!bCreative && (Player->XPLevel < Cost || Lap.IsEmpty() || Lap.Count < Id + 1)) return false;
	FMCRandom R((uint64)Player->EnchantSeed + Id);
	const TArray<FMCEnchantLevel> L = SelectEnchantments(R, S, Cost, false);
	if (L.Num() == 0) return false;
	const bool bBook = S.Item().Name == TEXT("book");
	if (bBook)
	{
		FMCItemStack Book = FMCItemStack::Of(TEXT("enchanted_book"), 1);
		Book.MutableExtra().StoredEnchants = L;
		S = Book;
	}
	else
	{
		for (const FMCEnchantLevel& E : L) S.AddEnchant(E.Id, E.Level);
	}
	if (!bCreative)
	{
		Player->GiveXPLevels(-(Id + 1));
		Lap.Count -= Id + 1;
		if (Lap.Count <= 0) Lap.Clear();
	}
	Player->EnchantSeed = Player->Rand().NextInt(INT_MAX);
	if (Player->World) Player->World->PlaySound(TEXT("enchantment_table_use"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5), 1.f, 0.9f + Player->Rand().NextFloat() * 0.1f);
	SlotsChanged();
	return true;
}

FMCItemStack FMCEnchantMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 2)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
	}
	else if (IsLapis(S))
	{
		if (!MoveItemStackTo(S, 1, 2, false)) return FMCItemStack();
	}
	else if (Local[0].IsEmpty())
	{
		Local[0] = S.Split(1);
	}
	else return FMCItemStack();
	SlotsChanged();
	return Orig;
}

void FMCEnchantMenu::Removed() { FMCMenu::Removed(); }

int32 FMCEnchantMenu::GetData(int32 Which) const
{
	if (Which >= 0 && Which < 3) return Costs[Which];
	if (Which >= 3 && Which < 6) return (int32)Hints[Which - 3];
	if (Which >= 6 && Which < 9) return HintLevels[Which - 6];
	if (Which == 9) return Bookshelves;
	return 0;
}

FString FMCEnchantMenu::GetText(int32 Which) const
{
	if (Which < 0 || Which > 2 || Hints[Which] == EMCEnchant::None) return FString();
	return FString::Printf(TEXT("%s %s . . . ?"), MCEnchants::Info(Hints[Which]).Name, *MCEnchants::Roman(HintLevels[Which]));
}

// ---------------------------------------------------------------------------------------------------------------------
// Anvil

FMCAnvilMenu::FMCAnvilMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Anvil, P)
{
	Title = TEXT("Repair & Name");
	Pos = InPos; bHasPos = true;
	Local.Init(3);
	LocalOutputSlot = 2;
	AddSlot(&Local, 0, 27, 47);
	AddSlot(&Local, 1, 76, 47);
	AddSlot(&Local, 2, 134, 47, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCAnvilMenu::SlotsChanged()
{
	FMCItemStack& A = Local[0];
	const FMCItemStack& B = Local[1];
	Local[2].Clear();
	Cost = 0;
	RepairItemCount = 0;
	if (A.IsEmpty()) return;
	FMCItemStack Out = A.Copy();
	const FMCItem& AI = A.Item();
	int32 Extra = 0;
	const int32 Prior = (A.Extra.IsValid() ? A.Extra->RepairCost : 0) + (!B.IsEmpty() && B.Extra.IsValid() ? B.Extra->RepairCost : 0);
	bool bBookOnItem = false;
	if (!B.IsEmpty())
	{
		const FMCItem& BI = B.Item();
		bBookOnItem = BI.Name == TEXT("enchanted_book") && B.Extra.IsValid() && B.Extra->StoredEnchants.Num() > 0;
		// repair with material
		if (A.IsDamageable() && !AI.RepairItem.IsNone() && BI.Name == AI.RepairItem)
		{
			int32 Dmg = A.Damage;
			const int32 Unit = FMath::Max(1, AI.MaxDamage / 4);
			int32 Used = 0;
			while (Dmg > 0 && Used < B.Count) { Dmg = FMath::Max(0, Dmg - Unit); ++Used; ++Extra; }
			if (Used == 0) return;
			Out.Damage = Dmg;
			RepairItemCount = Used;
		}
		else
		{
			if (!bBookOnItem && (B.Id != A.Id || !A.IsDamageable())) return;
			if (A.IsDamageable() && !bBookOnItem && B.Id == A.Id)
			{
				const int32 RemA = AI.MaxDamage - A.Damage, RemB = AI.MaxDamage - B.Damage;
				const int32 NewRem = RemA + RemB + AI.MaxDamage * 12 / 100;
				const int32 NewDmg = FMath::Max(0, AI.MaxDamage - NewRem);
				if (NewDmg < Out.Damage) { Out.Damage = NewDmg; Extra += 2; }
			}
			// merge enchantments
			const TArray<FMCEnchantLevel>& Src = bBookOnItem ? B.Extra->StoredEnchants : (B.Extra.IsValid() ? B.Extra->Enchants : TArray<FMCEnchantLevel>());
			const bool bOutBook = AI.Name == TEXT("enchanted_book");
			bool bAnyApplied = false, bAnyRejected = false;
			for (const FMCEnchantLevel& E : Src)
			{
				if (!bOutBook && !MCEnchants::CanApply(E.Id, AI) && !Player->IsCreative()) { bAnyRejected = true; continue; }
				const TArray<FMCEnchantLevel>& Existing = bOutBook ? (Out.Extra.IsValid() ? Out.Extra->StoredEnchants : TArray<FMCEnchantLevel>()) : (Out.Extra.IsValid() ? Out.Extra->Enchants : TArray<FMCEnchantLevel>());
				bool bCompatible = true;
				int32 Current = 0;
				for (const FMCEnchantLevel& X : Existing)
				{
					if (X.Id == E.Id) Current = X.Level;
					else if (!MCEnchants::Compatible(X.Id, E.Id)) bCompatible = false;
				}
				if (!bCompatible) { bAnyRejected = true; Extra += 1; continue; }
				int32 NewLevel = Current == E.Level ? E.Level + 1 : FMath::Max<int32>(Current, E.Level);
				NewLevel = FMath::Min<int32>(NewLevel, MCEnchants::Info(E.Id).MaxLevel);
				TArray<FMCEnchantLevel>& Dest = bOutBook ? Out.MutableExtra().StoredEnchants : Out.MutableExtra().Enchants;
				bool bFound = false;
				for (FMCEnchantLevel& X : Dest) if (X.Id == E.Id) { X.Level = (uint8)NewLevel; bFound = true; }
				if (!bFound) { FMCEnchantLevel N; N.Id = E.Id; N.Level = (uint8)NewLevel; Dest.Add(N); }
				Extra += EnchantCostWeight(E.Id, bBookOnItem) * NewLevel;
				bAnyApplied = true;
			}
			if (!bAnyApplied && bAnyRejected && Extra == 0) return;
		}
	}
	// rename
	bool bRenamed = false;
	FString Trimmed = ItemName.TrimStartAndEnd();
	const FString Current = A.Extra.IsValid() ? A.Extra->CustomName : FString();
	if (!Trimmed.IsEmpty() && Trimmed != AI.DisplayName && Trimmed != Current) { Out.MutableExtra().CustomName = Trimmed.Left(50); bRenamed = true; Extra += 1; }
	else if (Trimmed.IsEmpty() && !Current.IsEmpty()) { Out.MutableExtra().CustomName.Empty(); bRenamed = true; Extra += 1; }
	if (Extra <= 0) return;
	Cost = Prior + Extra;
	if (bRenamed && Extra == 1 && Cost >= 40) Cost = 39;
	if (Cost >= 40 && !Player->IsCreative()) { Cost = 40; return; } // too expensive
	if (!B.IsEmpty() || !bRenamed)
	{
		const int32 NewPrior = FMath::Max(A.Extra.IsValid() ? A.Extra->RepairCost : 0, !B.IsEmpty() && B.Extra.IsValid() ? B.Extra->RepairCost : 0) * 2 + 1;
		Out.MutableExtra().RepairCost = NewPrior;
	}
	Local[2] = Out;
}

bool FMCAnvilMenu::CanTake(int32 SlotIndex) const
{
	if (SlotIndex != 2) return true;
	return Cost > 0 && (Player->IsCreative() || Player->XPLevel >= Cost) && Cost < 40;
}

void FMCAnvilMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 2) return;
	if (!Player->IsCreative()) Player->GiveXPLevels(-Cost);
	Local[0].Clear();
	if (RepairItemCount > 0) { Local[1].Count -= RepairItemCount; if (Local[1].Count <= 0) Local[1].Clear(); }
	else Local[1].Clear();
	Cost = 0;
	ItemName.Empty();
	if (Player->World)
	{
		FMCWorld* W = Player->World;
		const FMCState S = W->GetState(Pos);
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (!Player->IsCreative() && W->Rand.NextFloat() < 0.12f)
		{
			const FString N = B.Name.ToString();
			const FName Next = N == TEXT("anvil") ? FName(TEXT("chipped_anvil")) : (N == TEXT("chipped_anvil") ? FName(TEXT("damaged_anvil")) : NAME_None);
			if (Next.IsNone()) { W->SetState(Pos, 0); W->PlaySound(TEXT("anvil_destroy"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5)); }
			else if (const FMCBlock* NB = FMCBlocks::Find(Next)) { W->SetState(Pos, NB->State(FMCBlocks::MetaOf(S)), MCSet_Default); W->PlaySound(TEXT("anvil_use"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5)); }
		}
		else W->PlaySound(TEXT("anvil_use"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5), 1.f, 0.9f + W->Rand.NextFloat() * 0.1f);
	}
	SlotsChanged();
}

FMCItemStack FMCAnvilMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 3)
	{
		if (SlotIndex == 2 && !CanTake(2)) return FMCItemStack();
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
		if (SlotIndex == 2) OnTakeOutput(2, Orig);
	}
	else if (!MoveItemStackTo(S, 0, 2, false)) return FMCItemStack();
	SlotsChanged();
	return Orig;
}

void FMCAnvilMenu::Removed() { FMCMenu::Removed(); }

// ---------------------------------------------------------------------------------------------------------------------
// Grindstone

FMCGrindstoneMenu::FMCGrindstoneMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Grindstone, P)
{
	Title = TEXT("Repair & Disenchant");
	Pos = InPos; bHasPos = true;
	Local.Init(3);
	LocalOutputSlot = 2;
	const int32 A = AddSlot(&Local, 0, 49, 19, MCSF_Filtered);
	const int32 B = AddSlot(&Local, 1, 49, 40, MCSF_Filtered);
	auto Filter = [](const FMCItemStack& S) { return S.IsDamageable() || S.IsEnchanted() || S.Item().Name == TEXT("enchanted_book"); };
	Slots[A].Filter = Filter; Slots[B].Filter = Filter;
	AddSlot(&Local, 2, 129, 34, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCGrindstoneMenu::SlotsChanged()
{
	const FMCItemStack& A = Local[0];
	const FMCItemStack& B = Local[1];
	Local[2].Clear();
	if (A.IsEmpty() && B.IsEmpty()) return;
	FMCItemStack Out;
	if (!A.IsEmpty() && !B.IsEmpty())
	{
		if (A.Id != B.Id || !A.IsDamageable()) return;
		Out = FMCItemStack(A.Id, 1);
		const int32 Max = A.Item().MaxDamage;
		const int32 Rem = (Max - A.Damage) + (Max - B.Damage) + Max * 5 / 100;
		Out.Damage = FMath::Max(0, Max - Rem);
	}
	else
	{
		const FMCItemStack& S = A.IsEmpty() ? B : A;
		if (S.Item().Name == TEXT("enchanted_book")) Out = FMCItemStack::Of(TEXT("book"), 1);
		else Out = S.Copy();
	}
	// keep curses only
	const FMCItemStack& Src = A.IsEmpty() ? B : A;
	if (Src.Extra.IsValid())
	{
		for (const FMCEnchantLevel& E : Src.Extra->Enchants) if (MCEnchants::Info(E.Id).bCurse) Out.AddEnchant(E.Id, E.Level);
		if (!Src.Extra->CustomName.IsEmpty()) Out.MutableExtra().CustomName = Src.Extra->CustomName;
	}
	if (Out.Extra.IsValid()) Out.Extra->RepairCost = 0;
	Local[2] = Out;
}

void FMCGrindstoneMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 2) return;
	int32 XP = 0;
	for (int32 i = 0; i < 2; ++i)
	{
		const FMCItemStack& S = Local[i];
		if (!S.Extra.IsValid()) continue;
		for (const FMCEnchantLevel& E : S.Extra->Enchants) if (!MCEnchants::Info(E.Id).bCurse) XP += MCEnchants::MinCost(E.Id, E.Level);
		for (const FMCEnchantLevel& E : S.Extra->StoredEnchants) if (!MCEnchants::Info(E.Id).bCurse) XP += MCEnchants::MinCost(E.Id, E.Level);
	}
	if (XP > 0 && Player->World) Player->World->SpawnXP(FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5), FMath::CeilToInt(XP / 2.f) + Player->Rand().NextInt(FMath::Max(1, XP / 2)));
	Local[0].Clear(); Local[1].Clear();
	if (Player->World) Player->World->PlaySound(TEXT("grindstone_use"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5));
	SlotsChanged();
}

FMCItemStack FMCGrindstoneMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 3)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
		if (SlotIndex == 2) OnTakeOutput(2, Orig);
	}
	else if (!MoveItemStackTo(S, 0, 2, false)) return FMCItemStack();
	SlotsChanged();
	return Orig;
}

void FMCGrindstoneMenu::Removed() { FMCMenu::Removed(); }

// ---------------------------------------------------------------------------------------------------------------------
// Stonecutter

FMCStonecutterMenu::FMCStonecutterMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Stonecutter, P)
{
	Title = TEXT("Stonecutter");
	Pos = InPos; bHasPos = true;
	Local.Init(2);
	LocalOutputSlot = 1;
	AddSlot(&Local, 0, 20, 33);
	AddSlot(&Local, 1, 143, 33, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCStonecutterMenu::SlotsChanged()
{
	const FMCItemStack& In = Local[0];
	TArray<const FMCStonecutRecipe*> R;
	if (!In.IsEmpty()) FMCRecipes::GetStonecutting(In.Id, R);
	TArray<FMCItemStack> NewOptions;
	for (const FMCStonecutRecipe* X : R) NewOptions.Add(FMCItemStack(X->Output, X->Count));
	bool bSame = NewOptions.Num() == Options.Num();
	for (int32 i = 0; bSame && i < Options.Num(); ++i) bSame = Options[i].Id == NewOptions[i].Id;
	if (!bSame) { Options = NewOptions; Selected = -1; }
	Local[1] = (Options.IsValidIndex(Selected) && !In.IsEmpty()) ? Options[Selected] : FMCItemStack();
}

bool FMCStonecutterMenu::ButtonClick(int32 Id)
{
	if (!Options.IsValidIndex(Id)) return false;
	Selected = Id;
	SlotsChanged();
	return true;
}

void FMCStonecutterMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 1) return;
	FMCItemStack& In = Local[0];
	if (!In.IsEmpty()) { In.Count -= 1; if (In.Count <= 0) In.Clear(); }
	if (Player->World) Player->World->PlaySound(TEXT("stonecutter_take_result"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5));
	SlotsChanged();
}

FMCItemStack FMCStonecutterMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 2)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
		if (SlotIndex == 1) OnTakeOutput(1, Orig);
	}
	else
	{
		TArray<const FMCStonecutRecipe*> R;
		FMCRecipes::GetStonecutting(S.Id, R);
		if (R.Num() == 0 || !MoveItemStackTo(S, 0, 1, false)) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

void FMCStonecutterMenu::Removed() { FMCMenu::Removed(); }

int32 FMCStonecutterMenu::GetData(int32 Which) const
{
	if (Which == 0) return Options.Num();
	if (Which == 1) return Selected;
	const int32 I = Which - 2;
	return Options.IsValidIndex(I) ? Options[I].Id : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Smithing table

FMCSmithingMenu::FMCSmithingMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Smithing, P)
{
	Title = TEXT("Upgrade Gear");
	Pos = InPos; bHasPos = true;
	Local.Init(4);
	LocalOutputSlot = 3;
	const int32 T = AddSlot(&Local, 0, 8, 48); Slots[T].EmptyIcon = TEXT("empty_slot_smithing_template");
	AddSlot(&Local, 1, 26, 48);
	const int32 A = AddSlot(&Local, 2, 44, 48); Slots[A].EmptyIcon = TEXT("empty_slot_ingot");
	AddSlot(&Local, 3, 98, 48, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCSmithingMenu::SlotsChanged()
{
	Local[3].Clear();
	if (Local[0].IsEmpty() || Local[1].IsEmpty() || Local[2].IsEmpty()) return;
	const FMCSmithRecipe* R = FMCRecipes::FindSmithing(Local[0].Id, Local[1].Id, Local[2].Id);
	if (!R) return;
	FMCItemStack Out = Local[1].Copy();
	Out.Id = R->Result;
	Out.Count = 1;
	Out.Damage = FMath::Min(Out.Damage, FMCItems::Get(R->Result).MaxDamage);
	Local[3] = Out;
}

void FMCSmithingMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 3) return;
	for (int32 i = 0; i < 3; ++i) { Local[i].Count -= 1; if (Local[i].Count <= 0) Local[i].Clear(); }
	if (Player->World) Player->World->PlaySound(TEXT("smithing_table_use"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5));
	SlotsChanged();
}

FMCItemStack FMCSmithingMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 4)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
		if (SlotIndex == 3) OnTakeOutput(3, Orig);
	}
	else
	{
		const FString N = S.Item().Name.ToString();
		int32 Target = N.EndsWith(TEXT("_smithing_template")) ? 0 : (N.EndsWith(TEXT("_ingot")) ? 2 : 1);
		if (!MoveItemStackTo(S, Target, Target + 1, false)) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

void FMCSmithingMenu::Removed() { FMCMenu::Removed(); }

// ---------------------------------------------------------------------------------------------------------------------
// Beacon

FMCBeaconMenu::FMCBeaconMenu(AMCPlayer* P, TSharedPtr<FMCBeaconEntity> B) : FMCMenu(EMCMenuType::Beacon, P), Beacon(B)
{
	BE = B;
	Title = TEXT("Beacon");
	PanelSize = FVector2D(230, 219);
	if (B.IsValid())
	{
		Pos = B->Pos; bHasPos = true;
		if (B->Payment.Num() < 1) B->Payment.Init(1);
		const int32 S = AddSlot(&B->Payment, 0, 136, 110, MCSF_Filtered | MCSF_Single);
		Slots[S].Filter = [](const FMCItemStack& St) { const FName N = St.Item().Name; return N == TEXT("iron_ingot") || N == TEXT("gold_ingot") || N == TEXT("emerald") || N == TEXT("diamond") || N == TEXT("netherite_ingot"); };
		PendingPrimary = B->Primary;
		PendingSecondary = B->Secondary;
		if (P->World) B->UpdateLevels(*P->World);
	}
	AddPlayerInventory(36, 137);
}

bool FMCBeaconMenu::CanPlace(int32 SlotIndex, const FMCItemStack& S) const { return FMCMenu::CanPlace(SlotIndex, S); }

bool FMCBeaconMenu::ButtonClick(int32 Id)
{
	if (!Beacon.IsValid()) return false;
	static const EMCEffect Primaries[] = { EMCEffect::Speed, EMCEffect::Haste, EMCEffect::Resistance, EMCEffect::JumpBoost, EMCEffect::Strength };
	static const int32 NeedLevel[] = { 1, 1, 2, 2, 3 };
	if (Id >= 1 && Id <= 5)
	{
		if (Beacon->Levels < NeedLevel[Id - 1]) return false;
		PendingPrimary = Primaries[Id - 1];
		return true;
	}
	if (Id == 100 || Id == 101)
	{
		if (Beacon->Levels < 4) return false;
		PendingSecondary = Id == 100 ? EMCEffect::Regeneration : PendingPrimary;
		return true;
	}
	if (Id == 999)
	{
		FMCItemStack& Pay = Beacon->Payment[0];
		if (Pay.IsEmpty() || PendingPrimary == EMCEffect::None) return false;
		Beacon->Primary = PendingPrimary;
		Beacon->Secondary = PendingSecondary;
		Pay.Clear();
		if (Player->World) { Player->World->MarkModified(Pos); Player->World->PlaySound(TEXT("beacon_power_select"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5)); }
		return true;
	}
	return false;
}

int32 FMCBeaconMenu::GetData(int32 Which) const
{
	if (!Beacon.IsValid()) return 0;
	switch (Which)
	{
	case 0: return Beacon->Levels;
	case 1: return (int32)PendingPrimary;
	case 2: return (int32)PendingSecondary;
	case 3: return (int32)Beacon->Primary;
	case 4: return (int32)Beacon->Secondary;
	default: return 0;
	}
}

FMCItemStack FMCBeaconMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex == 0) { if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack(); }
	else if (CanPlace(0, S) && Slots[0].Get().IsEmpty()) { Slots[0].Get() = S.Split(1); }
	else return FMCItemStack();
	return Orig;
}

void FMCBeaconMenu::Removed()
{
	// the payment slot belongs to the beacon but Minecraft returns it to the player
	if (Beacon.IsValid() && !Beacon->Payment[0].IsEmpty())
	{
		FMCItemStack S = Beacon->Payment[0];
		Beacon->Payment[0].Clear();
		if (!Player->AddItem(S)) Player->DropStack(S, false);
	}
	FMCMenu::Removed();
}

// ---------------------------------------------------------------------------------------------------------------------
// Loom (banner patterns: banners are represented by dyed wool items in this build)

FMCLoomMenu::FMCLoomMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Loom, P)
{
	Title = TEXT("Loom");
	Pos = InPos; bHasPos = true;
	Local.Init(4);
	LocalOutputSlot = 3;
	AddSlot(&Local, 0, 13, 26);
	AddSlot(&Local, 1, 33, 26);
	AddSlot(&Local, 2, 23, 45);
	AddSlot(&Local, 3, 143, 58, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCLoomMenu::SlotsChanged()
{
	Local[3].Clear();
	const FMCItemStack& Base = Local[0];
	const FMCItemStack& Dye = Local[1];
	if (Base.IsEmpty() || Dye.IsEmpty() || Pattern < 0) return;
	if (Dye.Item().Kind != EMCItemKind::Dye) return;
	// recolour wool / carpet / banners with the dye colour
	const FString BN = Base.Item().Name.ToString();
	const FString Col = Dye.Item().Name.ToString().LeftChop(4); // "<colour>_dye"
	FString Suffix;
	if (BN.EndsWith(TEXT("_wool"))) Suffix = TEXT("_wool");
	else if (BN.EndsWith(TEXT("_carpet"))) Suffix = TEXT("_carpet");
	else if (BN.EndsWith(TEXT("_banner"))) Suffix = TEXT("_banner");
	if (Suffix.IsEmpty()) return;
	FMCItemStack Out = FMCItemStack::Of(FName(*(Col + Suffix)), 1);
	if (!Out.IsEmpty()) Local[3] = Out;
}

bool FMCLoomMenu::ButtonClick(int32 Id)
{
	Pattern = Id;
	SlotsChanged();
	return true;
}

void FMCLoomMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 3) return;
	for (int32 i = 0; i < 2; ++i) { Local[i].Count -= 1; if (Local[i].Count <= 0) Local[i].Clear(); }
	if (Player->World) Player->World->PlaySound(TEXT("loom_take_result"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5));
	SlotsChanged();
}

FMCItemStack FMCLoomMenu::QuickMoveStack(int32 SlotIndex) { return FMCMenu::QuickMoveStack(SlotIndex); }
void FMCLoomMenu::Removed() { FMCMenu::Removed(); }

// ---------------------------------------------------------------------------------------------------------------------
// Cartography table

FMCCartographyMenu::FMCCartographyMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Cartography, P)
{
	Title = TEXT("Cartography Table");
	Pos = InPos; bHasPos = true;
	Local.Init(3);
	LocalOutputSlot = 2;
	AddSlot(&Local, 0, 15, 15);
	AddSlot(&Local, 1, 15, 52);
	AddSlot(&Local, 2, 145, 39, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCCartographyMenu::SlotsChanged()
{
	Local[2].Clear();
	const FMCItemStack& A = Local[0];
	const FMCItemStack& B = Local[1];
	if (A.IsEmpty() || B.IsEmpty() || A.Item().Name != TEXT("filled_map")) return;
	const FName BN = B.Item().Name;
	if (BN == TEXT("map")) { FMCItemStack Out = A.Copy(); Out.Count = 2; Local[2] = Out; }
	else if (BN == TEXT("paper") || BN == TEXT("glass_pane")) { Local[2] = A.Copy(); Local[2].Count = 1; }
}

void FMCCartographyMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 2) return;
	for (int32 i = 0; i < 2; ++i) { Local[i].Count -= 1; if (Local[i].Count <= 0) Local[i].Clear(); }
	if (Player->World) Player->World->PlaySound(TEXT("cartography_table_take_result"), FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5));
	SlotsChanged();
}

FMCItemStack FMCCartographyMenu::QuickMoveStack(int32 SlotIndex) { return FMCMenu::QuickMoveStack(SlotIndex); }
void FMCCartographyMenu::Removed() { FMCMenu::Removed(); }

// ---------------------------------------------------------------------------------------------------------------------
// Villager trading

FMCMerchantMenu::FMCMerchantMenu(AMCPlayer* P, AMCMob* InMerchant) : FMCMenu(EMCMenuType::Merchant, P), Merchant(InMerchant)
{
	Title = InMerchant ? InMerchant->GetDisplayName() : FString(TEXT("Trading"));
	PanelSize = FVector2D(276, 166);
	Local.Init(3);
	LocalOutputSlot = 2;
	AddSlot(&Local, 0, 136, 37);
	AddSlot(&Local, 1, 162, 37);
	AddSlot(&Local, 2, 220, 37, MCSF_Output);
	AddPlayerInventory(108, 84);
	if (InMerchant) InMerchant->TradingPlayer = P;
}

TArray<FMCTrade>* FMCMerchantMenu::Trades() const
{
	return Merchant.IsValid() ? &Merchant->Trades : nullptr;
}

bool FMCMerchantMenu::StillValid() const
{
	return Merchant.IsValid() && Merchant->IsAlive() && Player && Player->DistanceTo(Merchant.Get()) < 8.f;
}

void FMCMerchantMenu::SlotsChanged()
{
	Local[2].Clear();
	TArray<FMCTrade>* T = Trades();
	if (!T) return;
	auto Satisfies = [&](const FMCTrade& Tr)
	{
		if (Tr.Uses >= Tr.MaxUses) return false;
		const bool bA = !Local[0].IsEmpty() && Local[0].Id == Tr.CostA.Id && Local[0].Count >= Tr.CostA.Count;
		const bool bB = Tr.CostB.IsEmpty() || (!Local[1].IsEmpty() && Local[1].Id == Tr.CostB.Id && Local[1].Count >= Tr.CostB.Count);
		return bA && bB;
	};
	if (T->IsValidIndex(SelectedTrade) && Satisfies((*T)[SelectedTrade])) { Local[2] = (*T)[SelectedTrade].Result.Copy(); return; }
	for (int32 i = 0; i < T->Num(); ++i)
	{
		if (Satisfies((*T)[i])) { SelectedTrade = i; Local[2] = (*T)[i].Result.Copy(); return; }
	}
}

bool FMCMerchantMenu::ButtonClick(int32 Id)
{
	TArray<FMCTrade>* T = Trades();
	if (!T || !T->IsValidIndex(Id)) return false;
	SelectedTrade = Id;
	// return current inputs, then auto-fill from the inventory
	for (int32 i = 0; i < 2; ++i)
	{
		if (Local[i].IsEmpty()) continue;
		FMCItemStack S = Local[i];
		Local[i].Clear();
		if (!Player->AddItem(S)) Player->DropStack(S, false);
	}
	const FMCTrade& Tr = (*T)[Id];
	auto Fill = [&](int32 Slot, const FMCItemStack& Want)
	{
		if (Want.IsEmpty()) return;
		int32 Need = FMath::Min(Want.MaxStack(), Want.Count * 4);
		for (int32 s = 0; s < 36 && Need > 0; ++s)
		{
			FMCItemStack& PS = Player->Inventory.Slots[s];
			if (PS.IsEmpty() || PS.Id != Want.Id) continue;
			const int32 N = FMath::Min(Need, PS.Count);
			if (Local[Slot].IsEmpty()) Local[Slot] = FMCItemStack(Want.Id, 0);
			Local[Slot].Count += N;
			PS.Count -= N;
			if (PS.Count <= 0) PS.Clear();
			Need -= N;
		}
	};
	Fill(0, Tr.CostA);
	Fill(1, Tr.CostB);
	SlotsChanged();
	return true;
}

void FMCMerchantMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 2) return;
	TArray<FMCTrade>* T = Trades();
	if (!T || !T->IsValidIndex(SelectedTrade)) return;
	FMCTrade& Tr = (*T)[SelectedTrade];
	Local[0].Count -= Tr.CostA.Count; if (Local[0].Count <= 0) Local[0].Clear();
	if (!Tr.CostB.IsEmpty()) { Local[1].Count -= Tr.CostB.Count; if (Local[1].Count <= 0) Local[1].Clear(); }
	++Tr.Uses;
	if (Merchant.IsValid())
	{
		AMCMob* M = Merchant.Get();
		M->VillagerXP += Tr.XP;
		const int32 Thresholds[5] = { 0, 10, 70, 150, 250 };
		while (M->VillagerLevel < 5 && M->VillagerXP >= Thresholds[M->VillagerLevel])
		{
			++M->VillagerLevel;
			M->GenerateTrades();
			if (M->World) M->World->SpawnParticles(TEXT("happy_villager"), M->Pos + FVector(0, 0, M->Height), 12, 0.5f);
		}
		M->PlaySound(TEXT("villager_yes"), 1.f, 1.f);
	}
	Player->GiveXP(Player->Rand().Range(3, 6));
	SlotsChanged();
}

FMCItemStack FMCMerchantMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 3)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
		if (SlotIndex == 2) OnTakeOutput(2, Orig);
	}
	else if (!MoveItemStackTo(S, 0, 2, false)) return FMCItemStack();
	SlotsChanged();
	return Orig;
}

void FMCMerchantMenu::Removed()
{
	if (Merchant.IsValid()) Merchant->TradingPlayer.Reset();
	FMCMenu::Removed();
}

int32 FMCMerchantMenu::GetData(int32 Which) const
{
	TArray<FMCTrade>* T = Trades();
	if (Which == 0) return T ? T->Num() : 0;
	if (Which == 1) return SelectedTrade;
	if (Which == 2) return Merchant.IsValid() ? Merchant->VillagerLevel : 0;
	if (Which == 3) return Merchant.IsValid() ? Merchant->VillagerXP : 0;
	return 0;
}
