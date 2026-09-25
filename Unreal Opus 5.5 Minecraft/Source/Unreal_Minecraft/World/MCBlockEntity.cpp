#include "World/MCBlockEntity.h"
#include "World/MCWorld.h"
#include "Crafting/MCRecipes.h"
#include "Items/MCLoot.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCEntities.h"

// ---------------------------------------------------------------------------------------------------------------------
// Container

bool FMCContainer::IsEmpty() const
{
	for (const FMCItemStack& S : Slots) if (!S.IsEmpty()) return false;
	return true;
}

int32 FMCContainer::CountItem(FMCItemId Id) const
{
	int32 N = 0;
	for (const FMCItemStack& S : Slots) if (S.Id == Id) N += S.Count;
	return N;
}

int32 FMCContainer::Insert(FMCItemStack& Stack, int32 From, int32 To)
{
	if (Stack.IsEmpty()) return 0;
	if (To < 0 || To > Slots.Num()) To = Slots.Num();
	// merge first
	for (int32 i = From; i < To && Stack.Count > 0; ++i)
	{
		FMCItemStack& S = Slots[i];
		if (S.IsEmpty() || !S.CanStackWith(Stack)) continue;
		const int32 Space = S.MaxStack() - S.Count;
		if (Space <= 0) continue;
		const int32 Move = FMath::Min(Space, Stack.Count);
		S.Count += Move;
		Stack.Count -= Move;
	}
	for (int32 i = From; i < To && Stack.Count > 0; ++i)
	{
		FMCItemStack& S = Slots[i];
		if (!S.IsEmpty()) continue;
		const int32 Move = FMath::Min(Stack.MaxStack(), Stack.Count);
		S = Stack.Copy();
		S.Count = Move;
		Stack.Count -= Move;
	}
	if (Stack.Count <= 0) Stack.Clear();
	return Stack.Count;
}

bool FMCContainer::CanInsert(const FMCItemStack& Stack, int32 From, int32 To) const
{
	if (Stack.IsEmpty()) return true;
	if (To < 0 || To > Slots.Num()) To = Slots.Num();
	int32 Remaining = Stack.Count;
	for (int32 i = From; i < To && Remaining > 0; ++i)
	{
		const FMCItemStack& S = Slots[i];
		if (S.IsEmpty()) Remaining -= Stack.MaxStack();
		else if (S.CanStackWith(Stack)) Remaining -= (S.MaxStack() - S.Count);
	}
	return Remaining <= 0;
}

int32 FMCContainer::ComparatorSignal() const
{
	if (Slots.Num() == 0) return 0;
	float Fill = 0.f;
	bool bAny = false;
	for (const FMCItemStack& S : Slots)
	{
		if (S.IsEmpty()) continue;
		Fill += (float)S.Count / (float)FMath::Max(1, S.MaxStack());
		bAny = true;
	}
	Fill /= Slots.Num();
	return FMath::FloorToInt(Fill * 14.f) + (bAny ? 1 : 0);
}

void FMCContainer::Serialize(FArchive& Ar)
{
	int32 N = Slots.Num();
	Ar << N;
	if (Ar.IsLoading()) Slots.SetNum(FMath::Clamp(N, 0, 256));
	for (int32 i = 0; i < N && i < Slots.Num(); ++i) Ar << Slots[i];
}

// ---------------------------------------------------------------------------------------------------------------------
// Base block entity

TSharedPtr<FMCBlockEntity> FMCBlockEntity::Create(EMCBlockEntityType Type)
{
	switch (Type)
	{
	case EMCBlockEntityType::Chest: case EMCBlockEntityType::TrappedChest: case EMCBlockEntityType::Barrel: case EMCBlockEntityType::ShulkerBox:
	case EMCBlockEntityType::CopperChest: case EMCBlockEntityType::EnderChest:
		return MakeShared<FMCContainerEntity>(Type, 27);
	case EMCBlockEntityType::Dispenser: case EMCBlockEntityType::Dropper: case EMCBlockEntityType::Crafter:
		return MakeShared<FMCContainerEntity>(Type, 9);
	case EMCBlockEntityType::Jukebox: case EMCBlockEntityType::Lectern: case EMCBlockEntityType::DecoratedPot:
		return MakeShared<FMCContainerEntity>(Type, 1);
	case EMCBlockEntityType::Shelf: return MakeShared<FMCContainerEntity>(Type, 3);
	case EMCBlockEntityType::ChiseledBookshelf: return MakeShared<FMCContainerEntity>(Type, 6);
	case EMCBlockEntityType::Furnace: return MakeShared<FMCFurnaceEntity>(EMCFurnaceKind::Furnace);
	case EMCBlockEntityType::BlastFurnace: return MakeShared<FMCFurnaceEntity>(EMCFurnaceKind::Blast);
	case EMCBlockEntityType::Smoker: return MakeShared<FMCFurnaceEntity>(EMCFurnaceKind::Smoker);
	case EMCBlockEntityType::Hopper: return MakeShared<FMCHopperEntity>();
	case EMCBlockEntityType::BrewingStand: return MakeShared<FMCBrewingEntity>();
	case EMCBlockEntityType::Spawner: case EMCBlockEntityType::TrialSpawner: return MakeShared<FMCSpawnerEntity>();
	case EMCBlockEntityType::Campfire: return MakeShared<FMCCampfireEntity>();
	case EMCBlockEntityType::Beacon: return MakeShared<FMCBeaconEntity>();
	case EMCBlockEntityType::EndGateway: return MakeShared<FMCEndGatewayEntity>();
	case EMCBlockEntityType::Comparator: return MakeShared<FMCComparatorEntity>();
	case EMCBlockEntityType::DaylightDetector: return MakeShared<FMCDaylightEntity>();
	case EMCBlockEntityType::Beehive: return MakeShared<FMCBeehiveEntity>();
	default: return MakeShared<FMCBlockEntity>(Type);
	}
}

void FMCBlockEntity::Serialize(FArchive& Ar, int32 Version)
{
	Ar << CustomName;
	Ar << LootTable;
	Ar << LootSeed;
}

void FMCBlockEntity::DropContents(FMCWorld& W)
{
	FMCContainer* C = GetContainer();
	if (!C || Type == EMCBlockEntityType::ShulkerBox) return;
	UnpackLoot(W);
	for (FMCItemStack& S : C->Slots)
	{
		if (S.IsEmpty()) continue;
		// Minecraft scatters contents in random smaller stacks
		while (!S.IsEmpty())
		{
			const int32 N = FMath::Min(S.Count, W.Rand.Range(10, 30));
			FMCItemStack Part = S.Split(N);
			W.SpawnItem(FVector(Pos.X + 0.5 + W.Rand.FRange(-0.3f, 0.3f), Pos.Y + 0.5 + W.Rand.FRange(-0.3f, 0.3f), Pos.Z + 0.5), Part, true, 0.5f);
		}
	}
}

void FMCBlockEntity::UnpackLoot(FMCWorld& W)
{
	if (LootTable.IsNone()) return;
	FMCContainer* C = GetContainer();
	if (C) MCLoot::FillContainer(LootTable, LootSeed ^ W.Seed, *C);
	LootTable = NAME_None;
	W.MarkModified(Pos);
}

void FMCContainerEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	Inv.Serialize(Ar);
}

// ---------------------------------------------------------------------------------------------------------------------
// Furnaces

FMCFurnaceEntity::FMCFurnaceEntity(EMCFurnaceKind InKind)
	: FMCContainerEntity(InKind == EMCFurnaceKind::Furnace ? EMCBlockEntityType::Furnace : (InKind == EMCFurnaceKind::Blast ? EMCBlockEntityType::BlastFurnace : EMCBlockEntityType::Smoker), 3)
	, Kind(InKind)
{
}

void FMCFurnaceEntity::Tick(FMCWorld& W)
{
	const bool bWasLit = IsLit();
	bool bDirty = false;
	if (BurnTime > 0) --BurnTime;
	const uint8 Station = Kind == EMCFurnaceKind::Furnace ? MCSS_Furnace : (Kind == EMCFurnaceKind::Blast ? MCSS_Blast : MCSS_Smoker);
	FMCItemStack& In = Inv[0];
	FMCItemStack& Fuel = Inv[1];
	FMCItemStack& Out = Inv[2];
	const FMCSmeltRecipe* R = In.IsEmpty() ? nullptr : FMCRecipes::FindSmelting(In, Station);
	bool bCanSmelt = false;
	FMCItemStack Result;
	if (R)
	{
		Result = FMCItemStack(R->Output, R->Count);
		bCanSmelt = Out.IsEmpty() || (Out.CanStackWith(Result) && Out.Count + Result.Count <= Out.MaxStack());
	}
	if (!IsLit() && bCanSmelt && !Fuel.IsEmpty())
	{
		const int32 FuelTicks = FMCRecipes::FuelTicks(Fuel);
		if (FuelTicks > 0)
		{
			BurnTime = BurnDuration = Kind == EMCFurnaceKind::Furnace ? FuelTicks : FuelTicks / 2;
			const FMCItem& FI = Fuel.Item();
			if (FI.Name == TEXT("lava_bucket")) Fuel = FMCItemStack::Of(TEXT("bucket"), 1);
			else { Fuel.Count -= 1; if (Fuel.Count <= 0) Fuel.Clear(); }
			bDirty = true;
		}
	}
	if (IsLit() && bCanSmelt)
	{
		CookTotal = R ? (Kind == EMCFurnaceKind::Furnace ? R->CookTime : R->CookTime / 2) : 200;
		if (++CookTime >= CookTotal)
		{
			CookTime = 0;
			if (Out.IsEmpty()) Out = Result; else Out.Count += Result.Count;
			StoredXP += R->XP;
			// wet sponge + bucket in fuel slot -> water bucket
			if (In.Item().Name == TEXT("wet_sponge") && !Fuel.IsEmpty() && Fuel.Item().Name == TEXT("bucket")) Fuel = FMCItemStack::Of(TEXT("water_bucket"), 1);
			In.Count -= 1; if (In.Count <= 0) In.Clear();
			bDirty = true;
		}
	}
	else if (!IsLit() && CookTime > 0) CookTime = FMath::Max(0, CookTime - 2);
	else if (!bCanSmelt) CookTime = 0;

	if (bWasLit != IsLit())
	{
		const FMCState S = W.GetState(Pos);
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (B.Orient == EMCCubeOrient::Facing4Lit)
			W.SetState(Pos, B.State(MCMeta::SetBit(FMCBlocks::MetaOf(S), 2, IsLit())), MCSet_Render | MCSet_Light | MCSet_KeepEntity);
		bDirty = true;
	}
	if (bDirty) W.MarkModified(Pos);
}

int32 FMCFurnaceEntity::TakeXP()
{
	const int32 Whole = FMath::FloorToInt(StoredXP);
	const float Frac = StoredXP - Whole;
	StoredXP = 0.f;
	return Whole + (FMath::FRand() < Frac ? 1 : 0);
}

void FMCFurnaceEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCContainerEntity::Serialize(Ar, Version);
	Ar << BurnTime << BurnDuration << CookTime << CookTotal << StoredXP;
}

// ---------------------------------------------------------------------------------------------------------------------
// Hopper

namespace
{
	FMCContainer* ContainerAt(FMCWorld& W, const FMCBlockPos& P, TSharedPtr<FMCBlockEntity>* OutBE = nullptr)
	{
		TSharedPtr<FMCBlockEntity> BE = W.GetBlockEntityShared(P);
		if (!BE) return nullptr;
		if (BE->Type == EMCBlockEntityType::EnderChest) return nullptr;
		BE->UnpackLoot(W);
		if (OutBE) *OutBE = BE;
		return BE->GetContainer();
	}

	/** Slots a hopper may insert into / extract from for a given container type and side. */
	void SlotRange(EMCBlockEntityType T, bool bInsert, EMCFace FromSide, int32& OutFrom, int32& OutTo, int32 Num)
	{
		OutFrom = 0; OutTo = Num;
		if (T == EMCBlockEntityType::Furnace || T == EMCBlockEntityType::BlastFurnace || T == EMCBlockEntityType::Smoker)
		{
			if (bInsert) { if (FromSide == EMCFace::Up) { OutFrom = 0; OutTo = 1; } else { OutFrom = 1; OutTo = 2; } }
			else { OutFrom = 2; OutTo = 3; }
		}
		else if (T == EMCBlockEntityType::BrewingStand)
		{
			if (bInsert) { if (FromSide == EMCFace::Up) { OutFrom = 3; OutTo = 4; } else { OutFrom = 0; OutTo = 3; } }
			else { OutFrom = 0; OutTo = 3; }
		}
	}
}

void FMCHopperEntity::Tick(FMCWorld& W)
{
	if (--Cooldown > 0) return;
	const FMCState S = W.GetState(Pos);
	if (MCMeta::Bit(FMCBlocks::MetaOf(S), 3)) return; // disabled by redstone
	Cooldown = 0;
	bool bDidSomething = false;
	// push into the container we face
	const EMCFace Facing = MCMeta::Facing6(FMCBlocks::MetaOf(S));
	const FMCBlockPos Target = Pos.Offset(Facing);
	TSharedPtr<FMCBlockEntity> TBE;
	if (FMCContainer* T = ContainerAt(W, Target, &TBE))
	{
		int32 TFrom, TTo;
		SlotRange(TBE->Type, true, MC::Opposite(Facing), TFrom, TTo, T->Num());
		for (int32 i = 0; i < Inv.Num(); ++i)
		{
			if (Inv[i].IsEmpty()) continue;
			FMCItemStack One = Inv[i].Copy(); One.Count = 1;
			if (TBE->Type == EMCBlockEntityType::ShulkerBox && One.Item().Block && FMCBlocks::Get(One.Item().Block).Family == TEXT("shulker_box")) continue;
			if (T->Insert(One, TFrom, TTo) == 0)
			{
				Inv[i].Count -= 1; if (Inv[i].Count <= 0) Inv[i].Clear();
				bDidSomething = true;
				W.MarkModified(Target);
				break;
			}
		}
	}
	// pull from the container above
	const FMCBlockPos Above = Pos.Up();
	TSharedPtr<FMCBlockEntity> ABE;
	if (FMCContainer* A = ContainerAt(W, Above, &ABE))
	{
		int32 AFrom, ATo;
		SlotRange(ABE->Type, false, EMCFace::Down, AFrom, ATo, A->Num());
		for (int32 i = AFrom; i < ATo; ++i)
		{
			if ((*A)[i].IsEmpty()) continue;
			FMCItemStack One = (*A)[i].Copy(); One.Count = 1;
			if (Inv.Insert(One) == 0)
			{
				(*A)[i].Count -= 1; if ((*A)[i].Count <= 0) (*A)[i].Clear();
				bDidSomething = true;
				W.MarkModified(Above);
				break;
			}
		}
	}
	else if (!FMCBlocks::IsOpaque(W.GetState(Above)))
	{
		// collect item entities in the space above
		TArray<AMCEntity*> Ents;
		W.GetEntitiesInBox(FMCBox(Pos.X, Pos.Y, Pos.Z + 0.6875, Pos.X + 1, Pos.Y + 1, Pos.Z + 2), Ents);
		for (AMCEntity* E : Ents)
		{
			AMCItemEntity* IE = Cast<AMCItemEntity>(E);
			if (!IE || IE->bRemoved || IE->Stack.IsEmpty()) continue;
			const int32 Before = IE->Stack.Count;
			Inv.Insert(IE->Stack);
			if (IE->Stack.IsEmpty()) IE->Discard();
			if (!IE->Stack.IsEmpty() && IE->Stack.Count == Before) continue;
			bDidSomething = true;
			break;
		}
	}
	if (bDidSomething) { Cooldown = 8; W.MarkModified(Pos); }
}

void FMCHopperEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCContainerEntity::Serialize(Ar, Version);
	Ar << Cooldown;
}

// ---------------------------------------------------------------------------------------------------------------------
// Brewing stand

bool FMCBrewingEntity::CanBrew() const
{
	const FMCItemStack& Ing = Inv[3];
	if (Ing.IsEmpty() || !FMCRecipes::IsBrewIngredient(Ing.Id)) return false;
	for (int32 i = 0; i < 3; ++i)
	{
		const FMCItemStack& B = Inv[i];
		if (B.IsEmpty()) continue;
		const int32 From = B.Extra.IsValid() ? B.Extra->Potion : 0;
		const FName Name = B.Item().Name;
		if (Name == TEXT("potion") || Name == TEXT("splash_potion") || Name == TEXT("lingering_potion"))
		{
			if (FMCRecipes::FindBrew(Ing.Id, From) >= 0) return true;
			if (Ing.Item().Name == TEXT("gunpowder") && Name == TEXT("potion")) return true;
			if (Ing.Item().Name == TEXT("dragon_breath") && Name == TEXT("splash_potion")) return true;
		}
	}
	return false;
}

void FMCBrewingEntity::DoBrew()
{
	FMCItemStack& Ing = Inv[3];
	for (int32 i = 0; i < 3; ++i)
	{
		FMCItemStack& B = Inv[i];
		if (B.IsEmpty()) continue;
		const FName Name = B.Item().Name;
		const int32 From = B.Extra.IsValid() ? B.Extra->Potion : 0;
		if (Ing.Item().Name == TEXT("gunpowder") && Name == TEXT("potion")) { const uint8 P = (uint8)From; B = FMCItemStack::Of(TEXT("splash_potion"), 1); B.MutableExtra().Potion = P; continue; }
		if (Ing.Item().Name == TEXT("dragon_breath") && Name == TEXT("splash_potion")) { const uint8 P = (uint8)From; B = FMCItemStack::Of(TEXT("lingering_potion"), 1); B.MutableExtra().Potion = P; continue; }
		const int32 To = FMCRecipes::FindBrew(Ing.Id, From);
		if (To >= 0) B.MutableExtra().Potion = (uint8)To;
	}
	if (Ing.Item().Name == TEXT("dragon_breath")) Ing = FMCItemStack::Of(TEXT("glass_bottle"), Ing.Count);
	else { Ing.Count -= 1; if (Ing.Count <= 0) Ing.Clear(); }
}

void FMCBrewingEntity::Tick(FMCWorld& W)
{
	FMCItemStack& FuelS = Inv[4];
	if (Fuel <= 0 && !FuelS.IsEmpty() && FuelS.Item().Name == TEXT("blaze_powder"))
	{
		Fuel = 20;
		FuelS.Count -= 1; if (FuelS.Count <= 0) FuelS.Clear();
		W.MarkModified(Pos);
	}
	const bool bCan = CanBrew();
	if (BrewTime > 0)
	{
		--BrewTime;
		if (!bCan || Inv[3].Id != BrewingIngredient) { BrewTime = 0; W.MarkModified(Pos); return; }
		if (BrewTime == 0) { DoBrew(); W.PlaySound(TEXT("brewing_stand_brew"), Pos.Center() * MC::InvBlockSize); W.MarkModified(Pos); }
	}
	else if (bCan && Fuel > 0)
	{
		--Fuel;
		BrewTime = 400;
		BrewingIngredient = Inv[3].Id;
		W.MarkModified(Pos);
	}
}

void FMCBrewingEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCContainerEntity::Serialize(Ar, Version);
	Ar << BrewTime << Fuel << BrewingIngredient;
}

// ---------------------------------------------------------------------------------------------------------------------
// Spawner

void FMCSpawnerEntity::Tick(FMCWorld& W)
{
	Spin += 1.f;
	// active only when a player is within 16 blocks
	if (!W.Game || !W.Game->Player || W.Game->Player->World != &W) return;
	const FVector PP = W.Game->Player->Pos;
	if (FVector::DistSquared(PP, FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 0.5)) > 16.0 * 16.0) return;
	if (--Delay > 0) return;
	Delay = 200 + W.Rand.NextInt(600);
	// cap: max 6 of this mob nearby
	TArray<AMCEntity*> Near;
	W.GetEntitiesInBox(FMCBox(Pos.X - 4, Pos.Y - 4, Pos.Z - 1, Pos.X + 5, Pos.Y + 5, Pos.Z + 3), Near);
	int32 Count = 0;
	for (AMCEntity* E : Near) if (E && E->TypeId == Mob) ++Count;
	if (Count >= 6) return;
	const int32 Tries = 4;
	for (int32 i = 0; i < Tries; ++i)
	{
		const FVector SP(Pos.X + 0.5 + W.Rand.FRange(-4.f, 4.f), Pos.Y + 0.5 + W.Rand.FRange(-4.f, 4.f), Pos.Z + W.Rand.Range(-1, 1));
		const FMCBlockPos BP = FMCBlockPos::FromWorld(SP * MC::BlockSize);
		if (FMCBlocks::IsSolid(W.GetState(BP)) || FMCBlocks::IsSolid(W.GetState(BP.Up()))) continue;
		if (!FMCBlocks::IsSolid(W.GetState(BP.Down()))) continue;
		if (AMCEntity* E = W.SpawnMob(Mob, FVector(SP.X, SP.Y, BP.Z), false))
		{
			W.SpawnParticles(TEXT("smoke"), E->Pos + FVector(0, 0, 0.5), 10, 0.4f);
			W.SpawnParticles(TEXT("flame"), E->Pos + FVector(0, 0, 0.5), 5, 0.4f);
		}
	}
}

void FMCSpawnerEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	Ar << Mob << Delay;
}

// ---------------------------------------------------------------------------------------------------------------------
// Campfire

void FMCCampfireEntity::Tick(FMCWorld& W)
{
	const FMCState S = W.GetState(Pos);
	const bool bLit = !MCMeta::Bit(FMCBlocks::MetaOf(S), 2);
	for (int32 i = 0; i < 4; ++i)
	{
		FMCItemStack& It = Inv[i];
		if (It.IsEmpty()) { CookTimes[i] = 0; continue; }
		if (!bLit) { CookTimes[i] = FMath::Max(0, CookTimes[i] - 2); continue; }
		if (++CookTimes[i] >= 600)
		{
			const FMCSmeltRecipe* R = FMCRecipes::FindSmelting(It, MCSS_Campfire);
			const FMCItemStack Result = R ? FMCItemStack(R->Output, R->Count) : It;
			It.Clear();
			CookTimes[i] = 0;
			W.SpawnItem(FVector(Pos.X + 0.5, Pos.Y + 0.5, Pos.Z + 1.0), Result, true, 0.5f);
			W.MarkModified(Pos);
		}
	}
}

void FMCCampfireEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCContainerEntity::Serialize(Ar, Version);
	for (int32 i = 0; i < 4; ++i) Ar << CookTimes[i];
}

// ---------------------------------------------------------------------------------------------------------------------
// Beacon

void FMCBeaconEntity::UpdateLevels(FMCWorld& W)
{
	Levels = 0;
	for (int32 L = 1; L <= 4; ++L)
	{
		bool bFull = true;
		const int32 Z = Pos.Z - L;
		for (int32 dx = -L; dx <= L && bFull; ++dx)
			for (int32 dy = -L; dy <= L && bFull; ++dy)
			{
				const FMCBlock& B = FMCBlocks::GetByState(W.GetState(FMCBlockPos(Pos.X + dx, Pos.Y + dy, Z)));
				if (!B.HasTag(TEXT("beacon_base"))) bFull = false;
			}
		if (!bFull) break;
		Levels = L;
	}
}

void FMCBeaconEntity::Tick(FMCWorld& W)
{
	if (++Timer % 80 != 0) return;
	UpdateLevels(W);
	// beam needs a clear sky (transparent blocks allowed)
	bool bBeam = true;
	for (int32 Z = Pos.Z + 1; Z <= MC::MaxZ && bBeam; ++Z)
	{
		const FMCState S = W.GetState(FMCBlockPos(Pos.X, Pos.Y, Z));
		if (S != 0 && FMCBlocks::IsOpaque(S) && FMCBlocks::GetByState(S).Name != TEXT("bedrock")) bBeam = false;
	}
	if (Levels == 0 || !bBeam || Primary == EMCEffect::None) return;
	const float Range = Levels * 10.f + 10.f;
	const int32 Amp = (Levels >= 4 && Primary == Secondary) ? 1 : 0;
	const int32 Duration = (9 + Levels * 2) * 20;
	if (!W.Game || !W.Game->Player) return;
	AMCPlayer* P = W.Game->Player;
	if (P->World != &W) return;
	if (FMath::Abs(P->Pos.X - Pos.X) > Range || FMath::Abs(P->Pos.Y - Pos.Y) > Range) return;
	FMCEffectInstance E; E.Effect = Primary; E.Duration = Duration; E.Amplifier = (uint8)Amp; E.bAmbient = true;
	P->AddEffect(E);
	if (Levels >= 4 && Secondary != EMCEffect::None && Secondary != Primary)
	{
		FMCEffectInstance E2; E2.Effect = Secondary; E2.Duration = Duration; E2.bAmbient = true;
		P->AddEffect(E2);
	}
}

void FMCBeaconEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	uint8 A = (uint8)Primary, B = (uint8)Secondary;
	Ar << Levels << A << B;
	Primary = (EMCEffect)A; Secondary = (EMCEffect)B;
	if (Payment.Num() == 0) Payment.Init(1);
	Payment.Serialize(Ar);
}

// ---------------------------------------------------------------------------------------------------------------------

void FMCEndGatewayEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	Ar << Exit << bHasExit;
}

void FMCComparatorEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	Ar << Output;
}

void FMCDaylightEntity::Tick(FMCWorld& W)
{
	if (++Timer % 20 != 0) return;
	const FMCState S = W.GetState(Pos);
	if (FMCBlocks::GetByState(S).Name != TEXT("daylight_detector")) return;
	const bool bInverted = MCMeta::Bit(FMCBlocks::MetaOf(S), 4);
	int32 Sky = W.GetSkyLight(Pos);
	int32 Power = 0;
	if (W.Game && W.Dim == EMCDimension::Overworld)
	{
		Sky = FMath::Max(0, Sky - W.Game->GetSkyDarken());
		float Angle = W.Game->GetSunAngle() * 2.f * PI;
		if (!bInverted && Sky > 0)
		{
			const float Target = Angle < PI ? 0.f : 2.f * PI;
			Angle += (Target - Angle) * 0.2f;
			Power = FMath::RoundToInt(Sky * FMath::Cos(Angle));
		}
		else Power = Sky;
	}
	Power = FMath::Clamp(Power, 0, 15);
	if (bInverted) Power = 15 - Power;
	if ((FMCBlocks::MetaOf(S) & 15) != Power)
	{
		W.SetState(Pos, FMCBlocks::GetByState(S).State((uint8)((FMCBlocks::MetaOf(S) & 16) | Power)), MCSet_KeepEntity | MCSet_Render);
		W.NotifyNeighbors(Pos);
	}
}

void FMCBeehiveEntity::Tick(FMCWorld& W)
{
	if (++Timer % 2400 != 0 || Bees <= 0) return;
	Honey = FMath::Min(5, Honey + 1);
	W.MarkModified(Pos);
}

void FMCBeehiveEntity::Serialize(FArchive& Ar, int32 Version)
{
	FMCBlockEntity::Serialize(Ar, Version);
	Ar << Bees << Honey;
}
