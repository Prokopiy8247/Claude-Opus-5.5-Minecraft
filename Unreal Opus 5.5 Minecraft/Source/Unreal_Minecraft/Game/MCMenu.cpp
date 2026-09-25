// Container menus: Minecraft click semantics, inventory / crafting / chest / furnace / brewing / simple containers.
#include "Game/MCMenu.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Crafting/MCRecipes.h"
#include "World/MCWorld.h"

// ---------------------------------------------------------------------------------------------------------------------
// Base menu

FMCMenu::FMCMenu(EMCMenuType InType, AMCPlayer* InPlayer) : Type(InType), Player(InPlayer) {}

int32 FMCMenu::AddSlot(FMCContainer* C, int32 Index, float X, float Y, uint16 Flags)
{
	FMCSlot S;
	S.Container = C;
	S.Index = Index;
	S.UIPos = FVector2D(X, Y);
	S.Flags = Flags;
	return Slots.Add(S);
}

void FMCMenu::AddPlayerInventory(float X, float Y)
{
	PlayerSlotStart = Slots.Num();
	PlayerInvPos = FVector2D(X, Y);
	FMCContainer* Inv = &Player->Inventory;
	for (int32 Row = 0; Row < 3; ++Row)
		for (int32 Col = 0; Col < 9; ++Col)
			AddSlot(Inv, 9 + Row * 9 + Col, X + Col * 18, Y + Row * 18, MCSF_Player);
	for (int32 Col = 0; Col < 9; ++Col)
		AddSlot(Inv, Col, X + Col * 18, Y + 58, MCSF_Player | MCSF_Hotbar);
}

FMCItemStack& FMCMenu::Carried() { return Player->CarriedStack; }

void FMCMenu::DropCarried()
{
	if (!Carried().IsEmpty()) { Player->DropStack(Carried(), true); Carried().Clear(); }
}

int32 FMCMenu::MaxStackFor(int32 SlotIndex, const FMCItemStack& S) const
{
	const FMCSlot& Sl = Slots[SlotIndex];
	int32 M = FMath::Min(Sl.MaxStack, S.MaxStack());
	if (Sl.Flags & (MCSF_Head | MCSF_Chest | MCSF_Legs | MCSF_Feet | MCSF_Single)) M = 1;
	return M;
}

bool FMCMenu::CanPlace(int32 SlotIndex, const FMCItemStack& S) const
{
	if (!Slots.IsValidIndex(SlotIndex)) return false;
	const FMCSlot& Sl = Slots[SlotIndex];
	if (Sl.IsOutput()) return false;
	if (S.IsEmpty()) return true;
	const FMCItem& I = S.Item();
	auto IsEquip = [&](EMCArmorSlot A)
	{
		if (I.Kind == EMCItemKind::Armor && I.ArmorSlot == A) return true;
		if (A == EMCArmorSlot::Chest && I.Kind == EMCItemKind::Elytra) return true;
		if (A == EMCArmorSlot::Head && (I.Name == TEXT("carved_pumpkin") || I.Name.ToString().EndsWith(TEXT("_head")) || I.Name.ToString().EndsWith(TEXT("_skull")))) return true;
		return false;
	};
	if (Sl.Flags & MCSF_Head) return IsEquip(EMCArmorSlot::Head);
	if (Sl.Flags & MCSF_Chest) return IsEquip(EMCArmorSlot::Chest);
	if (Sl.Flags & MCSF_Legs) return IsEquip(EMCArmorSlot::Legs);
	if (Sl.Flags & MCSF_Feet) return IsEquip(EMCArmorSlot::Feet);
	if (Sl.Flags & MCSF_Fuel) return FMCRecipes::FuelTicks(S) > 0 || I.Name == TEXT("bucket");
	if ((Sl.Flags & MCSF_Filtered) && Sl.Filter) return Sl.Filter(S);
	return true;
}

bool FMCMenu::StillValid() const
{
	if (!Player || Player->Health <= 0.f) return false;
	if (bHasPos && Player->World)
	{
		const FVector C(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5);
		if (FVector::DistSquared(Player->Pos, C) > 64.0) return false;
		if (Player->World->GetState(Pos) == 0) return false;
	}
	return true;
}

void FMCMenu::Removed()
{
	// return menu-local items (crafting grids, anvil inputs...) except outputs
	for (int32 i = 0; i < Local.Num(); ++i)
	{
		if (i == LocalOutputSlot) { Local[i].Clear(); continue; }
		if (!Local[i].IsEmpty())
		{
			FMCItemStack S = Local[i];
			Local[i].Clear();
			if (!Player->AddItem(S)) Player->DropStack(S, false);
		}
	}
}

bool FMCMenu::MoveItemStackTo(FMCItemStack& Stack, int32 From, int32 To, bool bReverse)
{
	bool bMoved = false;
	if (Stack.IsEmpty()) return false;
	// merge into existing stacks
	if (Stack.MaxStack() > 1)
	{
		for (int32 k = 0; k < To - From && !Stack.IsEmpty(); ++k)
		{
			const int32 i = bReverse ? To - 1 - k : From + k;
			if (!Slots.IsValidIndex(i) || Slots[i].IsOutput()) continue;
			FMCItemStack& S = Slots[i].Get();
			if (S.IsEmpty() || !S.CanStackWith(Stack) || !CanPlace(i, Stack)) continue;
			const int32 Max = MaxStackFor(i, Stack);
			const int32 N = FMath::Min(Max - S.Count, Stack.Count);
			if (N <= 0) continue;
			S.Count += N;
			Stack.Count -= N;
			bMoved = true;
		}
	}
	// empty slots
	for (int32 k = 0; k < To - From && Stack.Count > 0; ++k)
	{
		const int32 i = bReverse ? To - 1 - k : From + k;
		if (!Slots.IsValidIndex(i) || Slots[i].IsOutput()) continue;
		FMCItemStack& S = Slots[i].Get();
		if (!S.IsEmpty() || !CanPlace(i, Stack)) continue;
		const int32 N = FMath::Min(MaxStackFor(i, Stack), Stack.Count);
		S = Stack.Copy();
		S.Count = N;
		Stack.Count -= N;
		bMoved = true;
	}
	if (Stack.Count <= 0) Stack.Clear();
	return bMoved;
}

FMCItemStack FMCMenu::QuickMoveStack(int32 SlotIndex)
{
	// default: container <-> player inventory
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	const int32 NumSlots = Slots.Num();
	if (SlotIndex < PlayerSlotStart)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, NumSlots, true)) return FMCItemStack();
	}
	else
	{
		if (!MoveItemStackTo(S, 0, PlayerSlotStart, false))
		{
			// within the player inventory: main <-> hotbar
			const int32 HotStart = PlayerSlotStart + 27;
			if (SlotIndex < HotStart) { if (!MoveItemStackTo(S, HotStart, HotStart + 9, false)) return FMCItemStack(); }
			else if (!MoveItemStackTo(S, PlayerSlotStart, HotStart, false)) return FMCItemStack();
		}
	}
	SlotsChanged();
	return Orig;
}

void FMCMenu::Click(int32 SlotIndex, int32 Button, EMCClick Mode, int32 HotbarKey)
{
	FMCItemStack& C = Carried();
	if (SlotIndex == -999)
	{
		// click outside the panel
		if (Mode == EMCClick::Pick && !C.IsEmpty())
		{
			if (Button == 0) DropCarried();
			else { FMCItemStack One = C.Split(1); Player->DropStack(One, true); }
		}
		return;
	}
	if (!Slots.IsValidIndex(SlotIndex)) return;
	FMCSlot& Sl = Slots[SlotIndex];
	FMCItemStack& S = Sl.Get();

	switch (Mode)
	{
	case EMCClick::Pick:
	{
		if (Sl.IsOutput())
		{
			if (S.IsEmpty() || !CanTake(SlotIndex)) return;
			if (C.IsEmpty())
			{
				const FMCItemStack Taken = S.Copy();
				C = S.Copy();
				S.Clear();
				OnTakeOutput(SlotIndex, Taken);
			}
			else if (C.CanStackWith(S) && C.Count + S.Count <= C.MaxStack())
			{
				const FMCItemStack Taken = S.Copy();
				C.Count += S.Count;
				S.Clear();
				OnTakeOutput(SlotIndex, Taken);
			}
			SlotsChanged();
			return;
		}
		if (C.IsEmpty())
		{
			if (S.IsEmpty() || !CanTake(SlotIndex)) return;
			const int32 N = Button == 0 ? S.Count : (S.Count + 1) / 2;
			C = S.Split(N);
		}
		else if (S.IsEmpty())
		{
			if (!CanPlace(SlotIndex, C)) return;
			const int32 N = Button == 0 ? FMath::Min(C.Count, MaxStackFor(SlotIndex, C)) : 1;
			S = C.Split(N);
		}
		else if (S.CanStackWith(C))
		{
			if (!CanPlace(SlotIndex, C)) return;
			const int32 Max = MaxStackFor(SlotIndex, C);
			const int32 N = FMath::Min(Button == 0 ? C.Count : 1, Max - S.Count);
			if (N > 0) { S.Count += N; C.Count -= N; if (C.Count <= 0) C.Clear(); }
		}
		else
		{
			if (!CanPlace(SlotIndex, C) || !CanTake(SlotIndex) || C.Count > MaxStackFor(SlotIndex, C)) return;
			Swap(S, C);
		}
		SlotsChanged();
		return;
	}
	case EMCClick::QuickMove:
	{
		if (!CanTake(SlotIndex)) return;
		if (Sl.IsOutput())
		{
			// craft as many as possible
			for (int32 Guard = 0; Guard < 64; ++Guard)
			{
				FMCItemStack& Out = Sl.Get();
				if (Out.IsEmpty()) break;
				FMCItemStack Taken = Out.Copy();
				FMCItemStack Moving = Out.Copy();
				if (!MoveItemStackTo(Moving, PlayerSlotStart, Slots.Num(), true) || !Moving.IsEmpty())
				{
					// did not fit completely: revert (Minecraft stops)
					if (!Moving.IsEmpty() && Moving.Count == Taken.Count) break;
				}
				Out.Clear();
				OnTakeOutput(SlotIndex, Taken);
				SlotsChanged();
				if (!Moving.IsEmpty()) { Player->DropStack(Moving, false); break; }
			}
			return;
		}
		QuickMoveStack(SlotIndex);
		return;
	}
	case EMCClick::Swap:
	{
		const int32 InvIndex = HotbarKey == 40 ? MCInv::Offhand : HotbarKey;
		if (InvIndex < 0) return;
		FMCItemStack& Hot = Player->Inventory.Slots[InvIndex];
		if (Sl.IsOutput())
		{
			if (S.IsEmpty() || !Hot.IsEmpty()) return;
			const FMCItemStack Taken = S.Copy();
			Hot = S.Copy();
			S.Clear();
			OnTakeOutput(SlotIndex, Taken);
			SlotsChanged();
			return;
		}
		if (!Hot.IsEmpty() && !CanPlace(SlotIndex, Hot)) return;
		if (!S.IsEmpty() && !CanTake(SlotIndex)) return;
		Swap(S, Hot);
		SlotsChanged();
		return;
	}
	case EMCClick::Clone:
	{
		if (Player->IsCreative() && C.IsEmpty() && !S.IsEmpty())
		{
			C = S.Copy();
			C.Count = C.MaxStack();
		}
		return;
	}
	case EMCClick::Throw:
	{
		if (!C.IsEmpty() || S.IsEmpty() || !CanTake(SlotIndex)) return;
		const FMCItemStack Taken = S.Copy();
		FMCItemStack D = Button == 1 ? S.Split(S.Count) : S.Split(1);
		Player->DropStack(D, true);
		if (Sl.IsOutput()) OnTakeOutput(SlotIndex, Taken);
		SlotsChanged();
		return;
	}
	case EMCClick::PickAll:
	{
		if (C.IsEmpty()) return;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 i = 0; i < Slots.Num() && C.Count < C.MaxStack(); ++i)
			{
				if (Slots[i].IsOutput()) continue;
				FMCItemStack& O = Slots[i].Get();
				if (O.IsEmpty() || !O.CanStackWith(C) || !CanTake(i)) continue;
				// first pass skips full stacks (Minecraft behaviour)
				if (Pass == 0 && O.Count == O.MaxStack()) continue;
				const int32 N = FMath::Min(C.MaxStack() - C.Count, O.Count);
				C.Count += N;
				O.Count -= N;
				if (O.Count <= 0) O.Clear();
			}
		}
		SlotsChanged();
		return;
	}
	default: break;
	}
}

void FMCMenu::QuickCraftDistribute(const TArray<int32>& SlotIndices, int32 Button)
{
	FMCItemStack& C = Carried();
	if (C.IsEmpty() || SlotIndices.Num() == 0) return;
	TArray<int32> Valid;
	for (int32 i : SlotIndices)
	{
		if (!Slots.IsValidIndex(i) || Slots[i].IsOutput()) continue;
		const FMCItemStack& S = Slots[i].Get();
		if (!S.IsEmpty() && !S.CanStackWith(C)) continue;
		if (!CanPlace(i, C)) continue;
		Valid.AddUnique(i);
	}
	if (Valid.Num() == 0) return;
	const int32 Total = C.Count;
	int32 Per = Button == 0 ? FMath::Max(1, Total / Valid.Num()) : 1;
	if (Button == 2 && Player->IsCreative()) Per = C.MaxStack();
	for (int32 i : Valid)
	{
		if (C.IsEmpty() && Button != 2) break;
		FMCItemStack& S = Slots[i].Get();
		const int32 Max = MaxStackFor(i, C);
		const int32 Have = S.IsEmpty() ? 0 : S.Count;
		const int32 N = FMath::Min(Per, Max - Have);
		if (N <= 0) continue;
		if (S.IsEmpty()) { S = C.Copy(); S.Count = 0; }
		S.Count += N;
		if (Button != 2) C.Count -= N;
	}
	if (C.Count <= 0) C.Clear();
	SlotsChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Crafting helpers

namespace
{
	/** Consume one of each grid item, leaving container remainders (buckets, bottles). */
	void ConsumeGrid(FMCContainer& Grid, int32 From, int32 Count, AMCPlayer* P)
	{
		for (int32 i = From; i < From + Count; ++i)
		{
			FMCItemStack& S = Grid[i];
			if (S.IsEmpty()) continue;
			const FName N = S.Item().Name;
			FName Remainder = NAME_None;
			if (N == TEXT("water_bucket") || N == TEXT("lava_bucket") || N == TEXT("milk_bucket") || N == TEXT("powder_snow_bucket")) Remainder = TEXT("bucket");
			else if (N == TEXT("honey_bottle") || N == TEXT("dragon_breath")) Remainder = TEXT("glass_bottle");
			S.Count -= 1;
			if (S.Count <= 0) S.Clear();
			if (!Remainder.IsNone())
			{
				FMCItemStack R = FMCItemStack::Of(Remainder, 1);
				if (S.IsEmpty()) S = R;
				else if (!P->AddItem(R)) P->DropStack(R, false);
			}
		}
	}

	void UpdateCraftResult(FMCContainer& Grid, int32 From, int32 W, int32 H, int32 OutIndex)
	{
		TArray<FMCItemStack> Cells;
		Cells.SetNum(W * H);
		for (int32 i = 0; i < W * H; ++i) Cells[i] = Grid[From + i];
		FMCItemStack Result;
		const FMCRecipe* R = FMCRecipes::FindCrafting(Cells.GetData(), W, H, Result);
		Grid[OutIndex] = R ? Result : FMCItemStack();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Player inventory (2x2 crafting, armour, offhand)

FMCInventoryMenu::FMCInventoryMenu(AMCPlayer* P) : FMCMenu(EMCMenuType::Inventory, P)
{
	Title = TEXT("Crafting");
	PanelSize = FVector2D(176, 166);
	Local.Init(5); // 0..3 grid, 4 result
	LocalOutputSlot = 4;
	CraftStart = Slots.Num();
	for (int32 y = 0; y < 2; ++y)
		for (int32 x = 0; x < 2; ++x)
			AddSlot(&Local, x + y * 2, 98 + x * 18, 18 + y * 18);
	ResultSlot = AddSlot(&Local, 4, 154, 28, MCSF_Output);
	ArmorStart = Slots.Num();
	const uint16 ArmorFlags[4] = { MCSF_Head, MCSF_Chest, MCSF_Legs, MCSF_Feet };
	const FName Empty[4] = { TEXT("empty_armor_slot_helmet"), TEXT("empty_armor_slot_chestplate"), TEXT("empty_armor_slot_leggings"), TEXT("empty_armor_slot_boots") };
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 Idx = AddSlot(&P->Inventory, 39 - i, 8, 8 + i * 18, ArmorFlags[i] | MCSF_Player);
		Slots[Idx].EmptyIcon = Empty[i];
	}
	AddPlayerInventory(8, 84);
	OffhandSlot = AddSlot(&P->Inventory, MCInv::Offhand, 77, 62, MCSF_Offhand | MCSF_Player);
	Slots[OffhandSlot].EmptyIcon = TEXT("empty_slot_shield");
	// the player slots start at the armour slots for shift-click purposes
	PlayerSlotStart = ArmorStart;
}

void FMCInventoryMenu::SlotsChanged()
{
	UpdateCraftResult(Local, 0, 2, 2, 4);
}

void FMCInventoryMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != ResultSlot) return;
	ConsumeGrid(Local, 0, 4, Player);
	Player->KnownRecipes.AddUnique(Taken.Item().Name);
	SlotsChanged();
}

FMCItemStack FMCInventoryMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	const int32 MainStart = ArmorStart + 4, HotStart = MainStart + 27, HotEnd = HotStart + 9;
	if (SlotIndex >= CraftStart && SlotIndex < ResultSlot)
	{
		if (!MoveItemStackTo(S, MainStart, HotEnd, false)) return FMCItemStack();
	}
	else if (SlotIndex >= ArmorStart && SlotIndex < MainStart)
	{
		if (!MoveItemStackTo(S, MainStart, HotEnd, false)) return FMCItemStack();
	}
	else if (SlotIndex == OffhandSlot)
	{
		if (!MoveItemStackTo(S, MainStart, HotEnd, false)) return FMCItemStack();
	}
	else
	{
		// armour first, then offhand for shields, then main <-> hotbar
		bool bDone = false;
		for (int32 a = 0; a < 4 && !bDone; ++a)
		{
			const int32 AS = ArmorStart + a;
			if (Slots[AS].Get().IsEmpty() && CanPlace(AS, S)) { bDone = MoveItemStackTo(S, AS, AS + 1, false); }
		}
		if (!bDone && S.Item().Kind == EMCItemKind::Shield && Slots[OffhandSlot].Get().IsEmpty()) bDone = MoveItemStackTo(S, OffhandSlot, OffhandSlot + 1, false);
		if (!bDone)
		{
			if (SlotIndex < HotStart) bDone = MoveItemStackTo(S, HotStart, HotEnd, false);
			else bDone = MoveItemStackTo(S, MainStart, HotStart, false);
		}
		if (!bDone) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

void FMCInventoryMenu::Removed()
{
	FMCMenu::Removed();
	Local.Init(5);
}

// ---------------------------------------------------------------------------------------------------------------------
// Crafting table

FMCCraftingMenu::FMCCraftingMenu(AMCPlayer* P, const FMCBlockPos& InPos) : FMCMenu(EMCMenuType::Crafting, P)
{
	Title = TEXT("Crafting");
	Pos = InPos; bHasPos = true;
	Local.Init(10);
	LocalOutputSlot = 9;
	for (int32 y = 0; y < 3; ++y)
		for (int32 x = 0; x < 3; ++x)
			AddSlot(&Local, x + y * 3, 30 + x * 18, 17 + y * 18);
	AddSlot(&Local, 9, 124, 35, MCSF_Output);
	AddPlayerInventory(8, 84);
}

void FMCCraftingMenu::SlotsChanged() { UpdateCraftResult(Local, 0, 3, 3, 9); }

void FMCCraftingMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 9) return;
	ConsumeGrid(Local, 0, 9, Player);
	Player->KnownRecipes.AddUnique(Taken.Item().Name);
	SlotsChanged();
}

FMCItemStack FMCCraftingMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 10)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
	}
	else
	{
		// player -> grid is not done by shift click in Minecraft; move between main and hotbar instead
		const int32 HotStart = PlayerSlotStart + 27;
		if (SlotIndex < HotStart) { if (!MoveItemStackTo(S, HotStart, HotStart + 9, false)) return FMCItemStack(); }
		else if (!MoveItemStackTo(S, PlayerSlotStart, HotStart, false)) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

void FMCCraftingMenu::Removed()
{
	FMCMenu::Removed();
}

bool FMCCraftingMenu::PlaceRecipe(const FMCRecipe& R, bool bMax)
{
	// return current grid contents
	for (int32 i = 0; i < 9; ++i)
	{
		if (Local[i].IsEmpty()) continue;
		FMCItemStack S = Local[i];
		Local[i].Clear();
		if (!Player->AddItem(S)) Player->DropStack(S, false);
	}
	const int32 Times = bMax ? 64 : 1;
	for (int32 t = 0; t < Times; ++t)
	{
		// check availability of one full set
		TArray<int32> Need;
		const int32 W = R.bShaped ? R.W : 3;
		for (int32 i = 0; i < R.Grid.Num(); ++i)
		{
			const FMCIngredient& Ing = R.Grid[i];
			if (Ing.IsEmpty()) continue;
			int32 Found = -1;
			for (int32 s = 0; s < 36 && Found < 0; ++s)
			{
				const FMCItemStack& PS = Player->Inventory.Slots[s];
				if (!PS.IsEmpty() && Ing.Matches(PS)) Found = s;
			}
			if (Found < 0 && !Player->IsCreative()) return t > 0;
			const int32 Cell = R.bShaped ? (i % W) + (i / W) * 3 : i;
			if (Cell >= 9) return t > 0;
			FMCItemStack& G = Local[Cell];
			const FMCItemId Id = Found >= 0 ? Player->Inventory.Slots[Found].Id : Ing.First();
			if (!G.IsEmpty() && (G.Id != Id || G.Count >= G.MaxStack())) return t > 0;
			if (G.IsEmpty()) G = FMCItemStack(Id, 0);
			G.Count += 1;
			if (Found >= 0 && !Player->IsCreative())
			{
				FMCItemStack& PS = Player->Inventory.Slots[Found];
				if (--PS.Count <= 0) PS.Clear();
			}
		}
	}
	SlotsChanged();
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Chests, barrels, shulker boxes, ender chest

FMCChestMenu::FMCChestMenu(AMCPlayer* P, EMCMenuType InType, FMCContainer* A, FMCContainer* B, int32 InRows, const FString& InTitle)
	: FMCMenu(InType, P), Rows(InRows)
{
	Title = InTitle;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < 9; ++c)
		{
			const int32 Idx = r * 9 + c;
			FMCContainer* Cont = A;
			int32 CI = Idx;
			if (A && Idx >= A->Num()) { Cont = B; CI = Idx - A->Num(); }
			if (!Cont || CI >= Cont->Num()) continue;
			const int32 S = AddSlot(Cont, CI, 8 + c * 18, 18 + r * 18);
			if (InType == EMCMenuType::Shulker)
			{
				Slots[S].Flags |= MCSF_Filtered;
				Slots[S].Filter = [](const FMCItemStack& St) { const FString N = St.Item().Name.ToString(); return !N.EndsWith(TEXT("shulker_box")); };
			}
		}
	}
	const float InvY = 18 + Rows * 18 + 13;
	AddPlayerInventory(8, InvY);
	PanelSize = FVector2D(176, InvY + 82);
}

FMCItemStack FMCChestMenu::QuickMoveStack(int32 SlotIndex)
{
	return FMCMenu::QuickMoveStack(SlotIndex);
}

void FMCChestMenu::Removed()
{
	FMCMenu::Removed();
	for (TSharedPtr<FMCBlockEntity>& V : Viewed)
	{
		if (!V.IsValid()) continue;
		if (FMCContainerEntity* CE = static_cast<FMCContainerEntity*>(V.Get()))
		{
			if (CE->GetContainer()) CE->Viewers = FMath::Max(0, CE->Viewers - 1);
		}
		if (Player && Player->World) Player->World->MarkModified(V->Pos);
	}
	if (Player && Player->World && bHasPos)
	{
		const FName N = FMCBlocks::GetByState(Player->World->GetState(Pos)).Name;
		const FName Snd = N == TEXT("barrel") ? FName(TEXT("barrel_close")) : (N.ToString().Contains(TEXT("shulker")) ? FName(TEXT("shulker_box_close")) : (N == TEXT("ender_chest") ? FName(TEXT("ender_chest_close")) : FName(TEXT("chest_close"))));
		Player->World->PlaySound(Snd, FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5), 0.5f, 0.9f + Player->World->Rand.NextFloat() * 0.1f);
		Player->World->NotifyNeighbors(Pos); // trapped chests / comparators
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Furnaces

FMCFurnaceMenu::FMCFurnaceMenu(AMCPlayer* P, TSharedPtr<FMCFurnaceEntity> F) : FMCMenu(EMCMenuType::Furnace, P), Furnace(F)
{
	BE = F;
	if (F.IsValid())
	{
		Pos = F->Pos; bHasPos = true;
		Type = F->Kind == EMCFurnaceKind::Blast ? EMCMenuType::BlastFurnace : (F->Kind == EMCFurnaceKind::Smoker ? EMCMenuType::Smoker : EMCMenuType::Furnace);
		Title = F->Kind == EMCFurnaceKind::Blast ? TEXT("Blast Furnace") : (F->Kind == EMCFurnaceKind::Smoker ? TEXT("Smoker") : TEXT("Furnace"));
		AddSlot(&F->Inv, 0, 56, 17);
		const int32 Fuel = AddSlot(&F->Inv, 1, 56, 53, MCSF_Fuel);
		Slots[Fuel].EmptyIcon = NAME_None;
		AddSlot(&F->Inv, 2, 116, 35, MCSF_Output);
	}
	AddPlayerInventory(8, 84);
}

bool FMCFurnaceMenu::CanPlace(int32 SlotIndex, const FMCItemStack& S) const
{
	return FMCMenu::CanPlace(SlotIndex, S);
}

float FMCFurnaceMenu::GetProgress(int32 Which) const
{
	if (!Furnace.IsValid()) return 0.f;
	if (Which == 0) return Furnace->CookTotal > 0 ? (float)Furnace->CookTime / Furnace->CookTotal : 0.f;
	return Furnace->BurnDuration > 0 ? (float)Furnace->BurnTime / Furnace->BurnDuration : 0.f;
}

void FMCFurnaceMenu::OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken)
{
	if (SlotIndex != 2 || !Furnace.IsValid()) return;
	const int32 XP = Furnace->TakeXP();
	if (XP > 0 && Player) Player->GiveXP(XP);
}

FMCItemStack FMCFurnaceMenu::QuickMoveStack(int32 SlotIndex)
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
	else
	{
		const uint8 Station = Type == EMCMenuType::BlastFurnace ? MCSS_Blast : (Type == EMCMenuType::Smoker ? MCSS_Smoker : MCSS_Furnace);
		bool bDone = false;
		if (FMCRecipes::FindSmelting(S, Station)) bDone = MoveItemStackTo(S, 0, 1, false);
		if (!bDone && FMCRecipes::FuelTicks(S) > 0) bDone = MoveItemStackTo(S, 1, 2, false);
		if (!bDone)
		{
			const int32 HotStart = PlayerSlotStart + 27;
			if (SlotIndex < HotStart) bDone = MoveItemStackTo(S, HotStart, HotStart + 9, false);
			else bDone = MoveItemStackTo(S, PlayerSlotStart, HotStart, false);
		}
		if (!bDone) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

// ---------------------------------------------------------------------------------------------------------------------
// Brewing stand

FMCBrewingMenu::FMCBrewingMenu(AMCPlayer* P, TSharedPtr<FMCBrewingEntity> B) : FMCMenu(EMCMenuType::Brewing, P), Stand(B)
{
	BE = B;
	Title = TEXT("Brewing Stand");
	if (B.IsValid())
	{
		Pos = B->Pos; bHasPos = true;
		const float BX[3] = { 56, 79, 102 }, BY[3] = { 51, 58, 51 };
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 S = AddSlot(&B->Inv, i, BX[i], BY[i], MCSF_Filtered | MCSF_Single);
			Slots[S].Filter = [](const FMCItemStack& St) { const FName N = St.Item().Name; return N == TEXT("potion") || N == TEXT("splash_potion") || N == TEXT("lingering_potion") || N == TEXT("glass_bottle"); };
			Slots[S].EmptyIcon = TEXT("empty_slot_potion");
		}
		const int32 Ing = AddSlot(&B->Inv, 3, 79, 17, MCSF_Filtered);
		Slots[Ing].Filter = [](const FMCItemStack& St) { return FMCRecipes::IsBrewIngredient(St.Id); };
		const int32 Fuel = AddSlot(&B->Inv, 4, 17, 17, MCSF_Filtered);
		Slots[Fuel].Filter = [](const FMCItemStack& St) { return St.Item().Name == TEXT("blaze_powder"); };
		Slots[Fuel].EmptyIcon = TEXT("empty_slot_blaze_powder");
	}
	AddPlayerInventory(8, 84);
}

bool FMCBrewingMenu::CanPlace(int32 SlotIndex, const FMCItemStack& S) const { return FMCMenu::CanPlace(SlotIndex, S); }

float FMCBrewingMenu::GetProgress(int32 Which) const
{
	if (!Stand.IsValid()) return 0.f;
	if (Which == 0) return Stand->BrewTime > 0 ? 1.f - Stand->BrewTime / 400.f : 0.f;
	return Stand->Fuel / 20.f;
}

FMCItemStack FMCBrewingMenu::QuickMoveStack(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return FMCItemStack();
	FMCItemStack& S = Slots[SlotIndex].Get();
	if (S.IsEmpty()) return FMCItemStack();
	const FMCItemStack Orig = S.Copy();
	if (SlotIndex < 5)
	{
		if (!MoveItemStackTo(S, PlayerSlotStart, Slots.Num(), true)) return FMCItemStack();
	}
	else
	{
		bool bDone = false;
		if (S.Item().Name == TEXT("blaze_powder")) bDone = MoveItemStackTo(S, 4, 5, false);
		if (!bDone && FMCRecipes::IsBrewIngredient(S.Id)) bDone = MoveItemStackTo(S, 3, 4, false);
		if (!bDone && CanPlace(0, S)) bDone = MoveItemStackTo(S, 0, 3, false);
		if (!bDone)
		{
			const int32 HotStart = PlayerSlotStart + 27;
			if (SlotIndex < HotStart) bDone = MoveItemStackTo(S, HotStart, HotStart + 9, false);
			else bDone = MoveItemStackTo(S, PlayerSlotStart, HotStart, false);
		}
		if (!bDone) return FMCItemStack();
	}
	SlotsChanged();
	return Orig;
}

// ---------------------------------------------------------------------------------------------------------------------
// Hopper, dispenser, dropper, crafter

FMCSimpleContainerMenu::FMCSimpleContainerMenu(AMCPlayer* P, EMCMenuType InType, TSharedPtr<FMCBlockEntity> InBE, const FString& InTitle)
	: FMCMenu(InType, P)
{
	BE = InBE;
	Title = InTitle;
	FMCContainer* C = InBE.IsValid() ? InBE->GetContainer() : nullptr;
	if (InBE.IsValid()) { Pos = InBE->Pos; bHasPos = true; }
	if (C)
	{
		if (InType == EMCMenuType::Hopper)
		{
			for (int32 i = 0; i < FMath::Min(5, C->Num()); ++i) AddSlot(C, i, 44 + i * 18, 20);
			AddPlayerInventory(8, 51);
			PanelSize = FVector2D(176, 133);
			return;
		}
		const float X0 = InType == EMCMenuType::Crafter ? 26 : 62;
		for (int32 y = 0; y < 3; ++y)
			for (int32 x = 0; x < 3; ++x)
				if (x + y * 3 < C->Num()) AddSlot(C, x + y * 3, X0 + x * 18, 17 + y * 18);
		if (InType == EMCMenuType::Crafter)
		{
			Local.Init(1);
			LocalOutputSlot = 0;
			AddSlot(&Local, 0, 134, 35, MCSF_Output);
		}
	}
	AddPlayerInventory(8, 84);
}

FMCItemStack FMCSimpleContainerMenu::QuickMoveStack(int32 SlotIndex)
{
	return FMCMenu::QuickMoveStack(SlotIndex);
}
