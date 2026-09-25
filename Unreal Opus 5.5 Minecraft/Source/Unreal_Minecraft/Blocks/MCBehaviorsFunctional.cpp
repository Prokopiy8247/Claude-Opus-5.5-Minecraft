// Functional blocks: workstations, containers, beds, campfires, cauldrons, beacons, anchors, jukeboxes,
// spawners, bells, cakes, candles, composters, lecterns, crafters, shelves.
#include "Blocks/MCBehaviorsInternal.h"
#include "Crafting/MCRecipes.h"
#include "Game/MCMenu.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

using namespace MCBeh;

namespace
{
	FString Pretty(const FMCBlock& B) { return B.DisplayName; }

	// ================================================================================================================
	/** Opens a menu created by a factory lambda (stations without inventories of their own). */
	class FStationBeh : public FMCBlockBehavior
	{
	public:
		enum EKind { Crafting, Enchanting, Anvil, Smithing, Stonecutter, Loom, Grindstone, Cartography };
		EKind Kind;
		bool bFacing = false;
		explicit FStationBeh(EKind K, bool bInFacing = false) : Kind(K), bFacing(bInFacing) {}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (Kind == Anvil) return B.State(MCMeta::FromFacing4(MC::RotateY(LookFacing(C), 1)));
			if (Kind == Grindstone) return B.State(MCMeta::FromFacing4(LookFacing(C)));
			if (bFacing || B.Orient == EMCCubeOrient::Facing4 || B.Model == EMCModel::Stonecutter) return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer()));
			return B.BaseState;
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			TSharedPtr<FMCMenu> M;
			switch (Kind)
			{
			case Crafting: M = MakeShared<FMCCraftingMenu>(Player, P); break;
			case Enchanting: M = MakeShared<FMCEnchantMenu>(Player, P); break;
			case Anvil: M = MakeShared<FMCAnvilMenu>(Player, P); break;
			case Smithing: M = MakeShared<FMCSmithingMenu>(Player, P); break;
			case Stonecutter: M = MakeShared<FMCStonecutterMenu>(Player, P); break;
			case Loom: M = MakeShared<FMCLoomMenu>(Player, P); break;
			case Grindstone: M = MakeShared<FMCGrindstoneMenu>(Player, P); break;
			case Cartography: M = MakeShared<FMCCartographyMenu>(Player, P); break;
			}
			if (M) Player->OpenMenu(M);
			return true;
		}
		// anvils fall
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { if (Kind == Anvil) CheckFall(W, P, S); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (Kind == Anvil && From == P.Down()) CheckFall(W, P, S);
		}
		static void CheckFall(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			const FMCState Below = W.GetState(P.Down());
			if (Below != 0 && !FMCBlocks::IsFluid(Below) && !FMCBlocks::IsReplaceable(Below)) return;
			if (!W.Game) return;
			W.SetState(P, 0);
			if (AMCFallingBlock* FB = W.Game->SpawnEntity<AMCFallingBlock>(&W, FVector(P.X + 0.5, P.Y + 0.5, P.Z)))
			{
				FB->State = S;
				FB->bHurtEntities = true;
				FB->InitEntity();
			}
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (Kind != Enchanting) return;
			// glyphs flowing from nearby bookshelves
			for (int32 i = 0; i < 2; ++i)
			{
				const int32 DX = R.Range(-2, 2), DY = R.Range(-2, 2), DZ = R.Range(0, 1);
				if (FMath::Abs(DX) < 2 && FMath::Abs(DY) < 2) continue;
				if (FMCBlocks::GetByState(W.GetState(FMCBlockPos(P.X + DX, P.Y + DY, P.Z + DZ))).Name != TEXT("bookshelf")) continue;
				if (R.NextInt(16) != 0) continue;
				W.SpawnParticles(TEXT("enchant"), FVector(P.X + 0.5 + DX, P.Y + 0.5 + DY, P.Z + DZ + 0.5), 1, 0.f,
					FVector(-DX * 0.05, -DY * 0.05, 0.02), FColor(200, 200, 255));
			}
		}
	};

	// ================================================================================================================
	class FFurnaceBeh : public FMCBlockBehavior
	{
	public:
		EMCFurnaceKind Kind;
		explicit FFurnaceBeh(EMCFurnaceKind K) : Kind(K) {}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer()));
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCFurnaceEntity>(Kind); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 2) ? 13 : 0; }
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (!MCMeta::Bit(FMCBlocks::MetaOf(S), 2)) return;
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const FIntVector& D = MC::FaceDir[(int32)F];
			const FVector Front(P.X + 0.5 + D.X * 0.52, P.Y + 0.5 + D.Y * 0.52, P.Z + R.FRange(0.1f, 0.45f));
			const FVector Side = FVector(-D.Y, D.X, 0) * R.FRange(-0.3f, 0.3f);
			W.SpawnParticles(TEXT("smoke"), Front + Side, 1, 0.f, FVector(0, 0, 0.01), FColor(80, 80, 80));
			if (Kind != EMCFurnaceKind::Smoker) W.SpawnParticles(TEXT("flame"), Front + Side, 1, 0.f, FVector::ZeroVector, FColor::White);
			if (R.Chance(0.1)) W.PlaySound(Kind == EMCFurnaceKind::Blast ? TEXT("blastfurnace_fire_crackle") : (Kind == EMCFurnaceKind::Smoker ? TEXT("smoker_smoke") : TEXT("furnace_fire_crackle")), P.Center() * MC::InvBlockSize, 1.f, 1.f);
		}
	};

	// ================================================================================================================
	/** Chests (normal, trapped, copper): single or double. Meta: facing bits 0-1, type bits 2-3 (0 single, 1/2 halves). */
	class FChestBeh : public FMCBlockBehavior
	{
	public:
		bool bTrapped = false;
		static int32 Type(FMCState S) { return (FMCBlocks::MetaOf(S) >> 2) & 3; }
		static EMCFace Facing(FMCState S) { return MCMeta::Facing4(FMCBlocks::MetaOf(S)); }
		static FMCBlockPos Partner(const FMCBlockPos& P, FMCState S)
		{
			const int32 T = Type(S);
			if (T == 1) return P.Offset(MC::RotateY(Facing(S), 1));
			if (T == 2) return P.Offset(MC::RotateY(Facing(S), 3));
			return P;
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const EMCFace F = C.FacingTowardsPlayer();
			uint8 M = MCMeta::FromFacing4(F);
			if (C.World && !C.bSneaking)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const FMCBlockPos N = C.Pos.Offset(MC::RotateY(F, Side == 0 ? 1 : 3));
					const FMCState NS = C.World->GetState(N);
					if (FMCBlocks::BlockOf(NS) == B.Id && Facing(NS) == F && Type(NS) == 0)
					{
						M |= (Side == 0 ? 1 : 2) << 2;
						break;
					}
				}
			}
			return B.State(M);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			const int32 T = Type(S);
			if (T == 0) return;
			const FMCBlockPos N = Partner(P, S);
			const FMCState NS = W.GetState(N);
			if (FMCBlocks::BlockOf(NS) != FMCBlocks::BlockOf(S)) return;
			W.SetState(N, WithMeta(NS, (uint8)((FMCBlocks::MetaOf(NS) & 3) | ((T == 1 ? 2 : 1) << 2))), MCSet_Render | MCSet_KeepEntity);
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (FMCBlocks::BlockOf(New) == FMCBlocks::BlockOf(Old)) return;
			const int32 T = Type(Old);
			if (T == 0) return;
			const FMCBlockPos N = Partner(P, Old);
			const FMCState NS = W.GetState(N);
			if (FMCBlocks::BlockOf(NS) == FMCBlocks::BlockOf(Old)) W.SetState(N, WithMeta(NS, (uint8)(FMCBlocks::MetaOf(NS) & 3)), MCSet_Render | MCSet_KeepEntity);
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override
		{
			return MakeShared<FMCContainerEntity>(bTrapped ? EMCBlockEntityType::TrappedChest : EMCBlockEntityType::Chest, 27);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			if (FMCBlocks::IsOpaque(W.GetState(P.Up()))) return true; // blocked lid
			if (Type(S) != 0 && FMCBlocks::IsOpaque(W.GetState(Partner(P, S).Up()))) return true;
			Player->OpenBlockContainer(P);
			return true;
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			if (!BE || !BE->GetContainer()) return 0;
			if (Type(S) != 0)
			{
				FMCBlockEntity* BE2 = W.GetBlockEntity(Partner(P, S));
				if (BE2 && BE2->GetContainer())
				{
					FMCContainer Both;
					Both.Slots = BE->GetContainer()->Slots;
					Both.Slots.Append(BE2->GetContainer()->Slots);
					return Both.ComparatorSignal();
				}
			}
			return BE->GetContainer()->ComparatorSignal();
		}
		virtual bool IsPowerSource(FMCState S) const override { return bTrapped; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			if (!bTrapped) return 0;
			FMCContainerEntity* BE = static_cast<FMCContainerEntity*>(W.GetBlockEntity(P));
			return BE ? FMath::Clamp(BE->Viewers, 0, 15) : 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return Dir == EMCFace::Down ? GetWeakPower(W, P, S, Dir) : 0;
		}
	};

	class FEnderChestBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer())); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			if (FMCBlocks::IsOpaque(W.GetState(P.Up()))) return true;
			Player->OpenMenu(MakeShared<FMCChestMenu>(Player, EMCMenuType::Chest, &Player->EnderInventory, nullptr, 3, TEXT("Ender Chest")));
			W.PlaySound(TEXT("ender_chest_open"), P.Center() * MC::InvBlockSize, 0.5f, W.Rand.FRange(0.9f, 1.0f));
			return true;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.3)) W.SpawnParticles(TEXT("portal"), FVector(P.X + 0.5 + R.FRange(-0.3f, 0.3f), P.Y + 0.5 + R.FRange(-0.3f, 0.3f), P.Z + R.FRange(0.1f, 0.8f)), 1, 0.f, FVector::ZeroVector, FColor(150, 60, 220));
		}
	};

	class FBarrelBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)MC::Opposite(LookFacing6(C)));
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCContainerEntity>(EMCBlockEntityType::Barrel, 27); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
	};

	/** Shulker boxes keep their contents when broken (stored in the dropped item). */
	class FShulkerBoxBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCContainerEntity>(EMCBlockEntityType::ShulkerBox, 27); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			FMCItemStack Item = FMCItemStack::Of(FMCBlocks::GetByState(S).Name, 1);
			if (FMCBlockEntity* BE = W.GetBlockEntity(P))
			{
				if (FMCContainer* C = BE->GetContainer())
				{
					if (!C->IsEmpty())
					{
						FBufferArchive Ar;
						C->Serialize(Ar);
						Item.MutableExtra().BlockEntityData = Ar;
						C->Slots.SetNum(0); C->Init(27); // prevent the generic "drop contents"
					}
				}
			}
			Out.Add(Item);
			return true;
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			// restore contents from the placed item
			if (!Player) return;
			const FMCItemStack& Held = Player->HeldConst();
			if (Held.IsEmpty() || !Held.Extra.IsValid() || Held.Extra->BlockEntityData.Num() == 0) return;
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			if (!BE || !BE->GetContainer()) return;
			FMemoryReader Ar(Held.Extra->BlockEntityData);
			BE->GetContainer()->Serialize(Ar);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
	};

	class FBrewingBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCBrewingEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.3)) W.SpawnParticles(TEXT("smoke"), FVector(P.X + 0.4 + R.NextFloat() * 0.2f, P.Y + 0.4 + R.NextFloat() * 0.2f, P.Z + 0.7), 1, 0.f, FVector(0, 0, 0.005), FColor(200, 200, 200));
		}
	};

	// ================================================================================================================
	class FBedBeh : public FMCBlockBehavior
	{
	public:
		static bool IsHead(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 2); }
		static EMCFace Facing(FMCState S) { return MCMeta::Facing4(FMCBlocks::MetaOf(S)); }
		static FMCBlockPos Other(const FMCBlockPos& P, FMCState S) { return IsHead(S) ? P.Offset(MC::Opposite(Facing(S))) : P.Offset(Facing(S)); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const EMCFace F = LookFacing(C);
			if (!C.World) return B.State(MCMeta::FromFacing4(F));
			const FMCBlockPos Head = C.Pos.Offset(F);
			const FMCState HS = C.World->GetState(Head);
			if (HS != 0 && !FMCBlocks::IsReplaceable(HS)) return 0;
			if (!MCBehaviorUtil::HasSolidTop(*C.World, Head.Down())) return 0;
			return B.State(MCMeta::FromFacing4(F));
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			W.SetState(P.Offset(Facing(S)), WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) | 4)), MCSet_Default | MCSet_NoSupport);
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (FMCBlocks::BlockOf(New) == FMCBlocks::BlockOf(Old)) return;
			const FMCBlockPos O = Other(P, Old);
			if (FMCBlocks::BlockOf(W.GetState(O)) == FMCBlocks::BlockOf(Old)) W.SetState(O, 0, MCSet_Default);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (!IsHead(S)) Out.Add(FMCItemStack::Of(FMCBlocks::GetByState(S).Name, 1));
			return true;
		}
		virtual float FallDamageMultiplier(FMCState S) const override { return 0.5f; }
		virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const override
		{
			if (E && !E->bSneaking && E->Vel.Z < 0) E->Vel.Z = -E->Vel.Z * 0.66;
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player || !W.Game) return false;
			const FMCBlockPos HeadPos = IsHead(S) ? P : Other(P, S);
			if (W.Dim != EMCDimension::Overworld)
			{
				// beds explode outside the overworld
				W.SetState(P, 0);
				W.SetState(Other(P, S), 0);
				W.Explode(HeadPos.Center() * MC::InvBlockSize, 5.f, true, true, nullptr);
				return true;
			}
			if (FMath::Abs(Player->Pos.X - (HeadPos.X + 0.5)) > 3 || FMath::Abs(Player->Pos.Y - (HeadPos.Y + 0.5)) > 3 || FMath::Abs(Player->Pos.Z - HeadPos.Z) > 2)
			{
				Player->ShowActionBar(TEXT("You may not rest now; the bed is too far away"));
				return true;
			}
			Player->SetSpawnPoint(HeadPos, W.Dim, false);
			const int64 T = W.Game->DayTime % 24000;
			const bool bNight = T >= 12542 && T <= 23460;
			if (!bNight && !W.Game->bThundering)
			{
				Player->ShowActionBar(TEXT("You can sleep only at night or during thunderstorms"));
				return true;
			}
			// monsters nearby
			TArray<AMCEntity*> Near;
			W.GetEntitiesInBox(FMCBox(HeadPos.X - 8, HeadPos.Y - 8, HeadPos.Z - 5, HeadPos.X + 8, HeadPos.Y + 8, HeadPos.Z + 5), Near);
			for (AMCEntity* E : Near)
			{
				AMCMob* M = Cast<AMCMob>(E);
				if (M && M->IsAlive() && M->Def && M->Def->bHostile && !Player->IsCreative())
				{
					Player->ShowActionBar(TEXT("You may not rest now; there are monsters nearby"));
					return true;
				}
			}
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 3)) { Player->ShowActionBar(TEXT("This bed is occupied")); return true; }
			Player->Sleep(HeadPos);
			return true;
		}
	};

	// ================================================================================================================
	class FCampfireBeh : public FMCBlockBehavior
	{
	public:
		static bool IsLit(FMCState S) { return !MCMeta::Bit(FMCBlocks::MetaOf(S), 2); }
		static bool IsSoul(FMCState S) { return FMCBlocks::GetByState(S).Name == TEXT("soul_campfire"); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const bool bWater = C.World && MCBehaviorUtil::IsWaterAt(*C.World, C.Pos);
			return B.State((uint8)(MCMeta::FromFacing4(C.FacingTowardsPlayer()) | (bWater ? 4 : 0)));
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCCampfireEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const FMCItem* I = HeldItem(Player);
			if (!I) return false;
			if (I->ToolType == EMCTool::Shovel && IsLit(S))
			{
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) | 4)), MCSet_Render | MCSet_Light | MCSet_KeepEntity);
				W.PlaySound(TEXT("fire_extinguish"), P.Center() * MC::InvBlockSize);
				Player->DamageHeld(1);
				return true;
			}
			if ((I->Name == FName(TEXT("flint_and_steel")) || I->Name == FName(TEXT("fire_charge"))) && !IsLit(S))
			{
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) & ~4)), MCSet_Render | MCSet_Light | MCSet_KeepEntity);
				W.PlaySound(TEXT("flint_and_steel_use"), P.Center() * MC::InvBlockSize);
				if (!Player->IsCreative()) { if (I->Name == FName(TEXT("fire_charge"))) Player->ConsumeHeld(1); else Player->DamageHeld(1); }
				return true;
			}
			// cook food
			if (const FMCSmeltRecipe* R = FMCRecipes::FindSmelting(Player->HeldConst(), MCSS_Campfire))
			{
				FMCCampfireEntity* BE = static_cast<FMCCampfireEntity*>(W.GetBlockEntity(P));
				if (!BE) return false;
				for (int32 i = 0; i < 4; ++i)
				{
					if (!BE->Inv[i].IsEmpty()) continue;
					BE->Inv[i] = Player->HeldConst().Copy();
					BE->Inv[i].Count = 1;
					BE->CookTimes[i] = 0;
					if (!Player->IsCreative()) Player->ConsumeHeld(1);
					W.MarkModified(P);
					return true;
				}
				(void)R;
			}
			return false;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || !IsLit(S) || E->bFireImmune || !E->IsLiving() || E->bSneaking) return;
			E->Hurt(FMCDamage::Of(TEXT("campfire")), IsSoul(S) ? 2.f : 1.f);
		}
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override
		{
			if (!IsLit(S) && Projectile && Projectile->IsOnFire())
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) & ~4)), MCSet_Render | MCSet_Light | MCSet_KeepEntity);
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return IsLit(S) ? Default : 0; }
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (!IsLit(S)) return;
			if (R.Chance(0.4)) W.SpawnParticles(TEXT("campfire_smoke"), FVector(P.X + 0.5 + R.FRange(-0.3f, 0.3f), P.Y + 0.5 + R.FRange(-0.3f, 0.3f), P.Z + 0.5), 1, 0.f, FVector(0, 0, 0.07), FColor(170, 170, 170));
			if (R.Chance(0.2)) W.SpawnParticles(IsSoul(S) ? TEXT("soul_fire_flame") : TEXT("lava_pop"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.4), 1, 0.2f, FVector(0, 0, 0.05), FColor(255, 180, 60));
			if (R.Chance(0.1)) W.PlaySound(TEXT("campfire_crackle"), P.Center() * MC::InvBlockSize, 0.5f + R.NextFloat(), R.FRange(0.6f, 1.3f));
		}
	};

	// ================================================================================================================
	class FCauldronBeh : public FMCBlockBehavior
	{
	public:
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const FMCItem* I = HeldItem(Player);
			if (!I) return false;
			const FName Block = FMCBlocks::GetByState(S).Name;
			const int32 Level = FMCBlocks::MetaOf(S) & 3;
			const FName N = I->Name;
			auto Replace = [&](const TCHAR* NewItem) { Player->ReplaceHeld(FMCItemStack::Of(NewItem, 1)); };
			if (N == TEXT("water_bucket"))
			{
				W.SetState(P, StateOf(TEXT("water_cauldron"), 3));
				if (!Player->IsCreative()) Replace(TEXT("bucket"));
				W.PlaySound(TEXT("bucket_empty"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (N == TEXT("lava_bucket"))
			{
				W.SetState(P, StateOf(TEXT("lava_cauldron"), 3));
				if (!Player->IsCreative()) Replace(TEXT("bucket"));
				W.PlaySound(TEXT("bucket_empty_lava"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (N == TEXT("bucket") && Level == 3 && Block != TEXT("cauldron"))
			{
				W.SetState(P, StateOf(TEXT("cauldron")));
				if (!Player->IsCreative()) Replace(Block == TEXT("lava_cauldron") ? TEXT("lava_bucket") : TEXT("water_bucket"));
				W.PlaySound(Block == TEXT("lava_cauldron") ? TEXT("bucket_fill_lava") : TEXT("bucket_fill"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (N == TEXT("glass_bottle") && Block == TEXT("water_cauldron") && Level > 0)
			{
				FMCItemStack Potion = FMCItemStack::Of(TEXT("potion"), 1);
				Potion.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(TEXT("water")));
				if (!Player->IsCreative()) { Player->ConsumeHeld(1); Player->GiveItem(Potion); }
				W.SetState(P, Level > 1 ? StateOf(TEXT("water_cauldron"), (uint8)(Level - 1)) : StateOf(TEXT("cauldron")));
				W.PlaySound(TEXT("bottle_fill"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (N == TEXT("potion") && (Block == TEXT("cauldron") || (Block == TEXT("water_cauldron") && Level < 3)))
			{
				W.SetState(P, StateOf(TEXT("water_cauldron"), (uint8)FMath::Min(3, Level + 1)));
				if (!Player->IsCreative()) Replace(TEXT("glass_bottle"));
				W.PlaySound(TEXT("bottle_empty"), P.Center() * MC::InvBlockSize);
				return true;
			}
			// wash dyed leather / banners
			if (Block == TEXT("water_cauldron") && Level > 0 && Player->HeldConst().Extra.IsValid() && Player->HeldConst().Extra->Color != 255)
			{
				Player->Held().MutableExtra().Color = 255;
				W.SetState(P, Level > 1 ? StateOf(TEXT("water_cauldron"), (uint8)(Level - 1)) : StateOf(TEXT("cauldron")));
				return true;
			}
			return false;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			const FName Block = FMCBlocks::GetByState(S).Name;
			if (Block == TEXT("water_cauldron") && E->IsOnFire())
			{
				E->Extinguish();
				const int32 Level = FMCBlocks::MetaOf(S) & 3;
				W.SetState(P, Level > 1 ? StateOf(TEXT("water_cauldron"), (uint8)(Level - 1)) : StateOf(TEXT("cauldron")));
			}
			else if (Block == TEXT("lava_cauldron") && !E->bFireImmune)
			{
				E->SetOnFire(15);
				if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("lava")), 4.f);
			}
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FName Block = FMCBlocks::GetByState(S).Name;
			if (Block == TEXT("cauldron")) return 0;
			return FMCBlocks::MetaOf(S) & 3;
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("cauldron"), 1));
			return true;
		}
	};

	// ================================================================================================================
	class FBeaconBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCBeaconEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			TSharedPtr<FMCBlockEntity> BE = W.GetBlockEntityShared(P);
			if (!BE || BE->Type != EMCBlockEntityType::Beacon)
			{
				BE = MakeShared<FMCBeaconEntity>();
				W.SetBlockEntity(P, BE);
			}
			Player->OpenMenu(MakeShared<FMCBeaconMenu>(Player, StaticCastSharedPtr<FMCBeaconEntity>(BE)));
			return true;
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			if (!W.GetBlockEntity(P)) W.SetBlockEntity(P, MakeShared<FMCBeaconEntity>());
		}
	};

	class FRespawnAnchorBeh : public FMCBlockBehavior
	{
	public:
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const int32 Charges = FMCBlocks::MetaOf(S) & 7;
			if (HeldIs(Player, TEXT("glowstone")) && Charges < 4)
			{
				W.SetState(P, WithMeta(S, (uint8)(Charges + 1)), MCSet_Render | MCSet_Light);
				if (!Player->IsCreative()) Player->ConsumeHeld(1);
				W.PlaySound(TEXT("respawn_anchor_charge"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (Charges == 0) return false;
			if (W.Dim != EMCDimension::Nether)
			{
				W.SetState(P, 0);
				W.Explode(P.Center() * MC::InvBlockSize, 5.f, true, true, nullptr);
				return true;
			}
			Player->SetSpawnPoint(P, W.Dim, false);
			Player->ShowActionBar(TEXT("Respawn point set"));
			W.PlaySound(TEXT("respawn_anchor_set_spawn"), P.Center() * MC::InvBlockSize);
			return true;
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override
		{
			static const uint8 L[5] = { 0, 3, 7, 11, 15 };
			return L[FMath::Min<int32>(FMCBlocks::MetaOf(S) & 7, 4)];
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return FMath::FloorToInt((FMCBlocks::MetaOf(S) & 7) / 4.f * 15.f);
		}
	};

	class FJukeboxBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCContainerEntity>(EMCBlockEntityType::Jukebox, 1); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			if (!BE) { W.SetBlockEntity(P, MakeShared<FMCContainerEntity>(EMCBlockEntityType::Jukebox, 1)); BE = W.GetBlockEntity(P); }
			FMCContainer* C = BE ? BE->GetContainer() : nullptr;
			if (!C) return false;
			if (!(*C)[0].IsEmpty())
			{
				MCBehaviorUtil::PopItem(W, P.Up(), (*C)[0]);
				(*C)[0].Clear();
				W.PlaySound(TEXT("music_stop"), P.Center() * MC::InvBlockSize);
				return true;
			}
			const FMCItem* I = HeldItem(Player);
			if (!I || I->Kind != EMCItemKind::Music) return false;
			(*C)[0] = Player->HeldConst().Copy();
			(*C)[0].Count = 1;
			if (!Player->IsCreative()) Player->ConsumeHeld(1);
			W.PlaySound(FName(*(FString(TEXT("music_")) + I->Name.ToString())), P.Center() * MC::InvBlockSize, 4.f, 1.f);
			if (W.Game) W.Game->ShowActionBar(FString::Printf(TEXT("Now Playing: %s"), *I->DisplayName));
			W.MarkModified(P);
			return true;
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() && !(*BE->GetContainer())[0].IsEmpty() ? 15 : 0;
		}
	};

	class FSpawnerBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCSpawnerEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const FMCItem* I = HeldItem(Player);
			if (!I || I->Kind != EMCItemKind::SpawnEgg) return false;
			FMCSpawnerEntity* BE = static_cast<FMCSpawnerEntity*>(W.GetBlockEntity(P));
			if (!BE || BE->Type != EMCBlockEntityType::Spawner) return false;
			BE->Mob = I->SpawnMob;
			BE->Delay = 20;
			W.MarkModified(P);
			if (!Player->IsCreative()) Player->ConsumeHeld(1);
			return true;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.5)) W.SpawnParticles(TEXT("flame"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + R.NextFloat()), 1, 0.f, FVector::ZeroVector, FColor::White);
			if (R.Chance(0.3)) W.SpawnParticles(TEXT("smoke"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + R.NextFloat()), 1, 0.f, FVector::ZeroVector, FColor(90, 90, 90));
		}
	};

	class FBellBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(MCMeta::FromFacing4(LookFacing(C))); }
		static void Ring(FMCWorld& W, const FMCBlockPos& P)
		{
			W.PlaySound(TEXT("bell_ring"), P.Center() * MC::InvBlockSize, 2.f, 1.f);
			// villagers run home, raiders glow
			TArray<AMCEntity*> Near;
			W.GetEntitiesInBox(FMCBox(P.X - 48, P.Y - 48, P.Z - 48, P.X + 48, P.Y + 48, P.Z + 48), Near);
			for (AMCEntity* E : Near)
			{
				AMCMob* M = Cast<AMCMob>(E);
				if (!M || !M->Def) continue;
				if (M->Def->AI == EMCMobAI::Illager && M->DistanceSqTo(FVector(P.X, P.Y, P.Z)) < 32 * 32)
				{
					FMCEffectInstance G; G.Effect = EMCEffect::Glowing; G.Duration = 60; M->AddEffect(G);
				}
				if (M->Def->AI == EMCMobAI::Villager) M->PanicTime = 200;
			}
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override { Ring(W, P); return true; }
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override { Ring(W, P); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			static TSet<FMCBlockPos> Powered;
			if (bPowered && !Powered.Contains(P)) { Powered.Add(P); Ring(W, P); }
			else if (!bPowered) Powered.Remove(P);
		}
	};

	class FCakeBeh : public FMCBlockBehavior
	{
	public:
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			// candles on cake
			if (HeldHasTag(Player, TEXT("candles")) && FMCBlocks::MetaOf(S) == 0) return false;
			if (!Player->CanEat(false) && !Player->IsCreative()) return false;
			Player->FoodLevel = FMath::Min(20, Player->FoodLevel + 2);
			Player->Saturation = FMath::Min((float)Player->FoodLevel, Player->Saturation + 2 * 0.1f * 2.f);
			W.PlaySound(TEXT("player_eat"), P.Center() * MC::InvBlockSize, 1.f, W.Rand.FRange(0.8f, 1.2f));
			const int32 Bites = FMCBlocks::MetaOf(S) & 7;
			if (Bites >= 6) W.SetState(P, 0);
			else W.SetState(P, WithMeta(S, (uint8)(Bites + 1)), MCSet_Render);
			return true;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return Below != 0 && FMCBlocks::IsSolid(Below);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.DestroyBlock(P, false);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return (7 - (FMCBlocks::MetaOf(S) & 7)) * 2; }
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override { return true; }
	};

	class FCandleBeh : public FMCBlockBehavior
	{
	public:
		static bool IsLit(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 2); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (FMCBlocks::BlockOf(C.Existing) == B.Id)
			{
				const int32 Count = (FMCBlocks::MetaOf(C.Existing) & 3) + 1;
				if (Count >= 4) return 0;
				return B.State((uint8)((FMCBlocks::MetaOf(C.Existing) & 4) | Count));
			}
			if (!C.World || !MCBehaviorUtil::HasSolidTop(*C.World, C.Pos.Down())) return 0;
			return B.State(0);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			if (HeldIs(Player, TEXT("flint_and_steel")) && !IsLit(S))
			{
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) | 4)), MCSet_Render | MCSet_Light);
				W.PlaySound(TEXT("flint_and_steel_use"), P.Center() * MC::InvBlockSize);
				if (!Player->IsCreative()) Player->DamageHeld(1);
				return true;
			}
			if (MCBehaviorUtil::HandEmpty(Player) && IsLit(S))
			{
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) & ~4)), MCSet_Render | MCSet_Light);
				W.PlaySound(TEXT("candle_extinguish"), P.Center() * MC::InvBlockSize);
				return true;
			}
			return false;
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return IsLit(S) ? (uint8)(3 * ((FMCBlocks::MetaOf(S) & 3) + 1)) : 0; }
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(FMCBlocks::GetByState(S).Name, (FMCBlocks::MetaOf(S) & 3) + 1));
			return true;
		}
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override
		{
			if (Projectile && Projectile->IsOnFire() && !IsLit(S)) W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) | 4)), MCSet_Render | MCSet_Light);
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (IsLit(S) && R.Chance(0.2)) W.SpawnParticles(TEXT("small_flame"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.55), 1, 0.05f, FVector::ZeroVector, FColor::White);
		}
	};

	class FComposterBeh : public FMCBlockBehavior
	{
	public:
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const int32 Level = FMCBlocks::MetaOf(S) & 15;
			if (Level >= 8)
			{
				MCBehaviorUtil::PopItem(W, P.Up(), FMCItemStack::Of(TEXT("bone_meal"), 1));
				W.SetState(P, WithMeta(S, 0), MCSet_Render);
				W.PlaySound(TEXT("composter_empty"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (!Player || Level >= 7) return Level >= 7;
			const FMCItemStack& Held = Player->HeldConst();
			int32 Chance = 0;
			if (Held.IsEmpty() || !FMCRecipes::IsCompostable(Held.Id, Chance)) return false;
			if (!Player->IsCreative()) Player->ConsumeHeld(1);
			const bool bRaise = Level == 0 || W.Rand.NextInt(100) < Chance;
			if (bRaise)
			{
				W.SetState(P, WithMeta(S, (uint8)(Level + 1)), MCSet_Render);
				if (Level + 1 == 7) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 20);
				W.PlaySound(TEXT("composter_fill_success"), P.Center() * MC::InvBlockSize);
			}
			else W.PlaySound(TEXT("composter_fill"), P.Center() * MC::InvBlockSize);
			W.SpawnParticles(TEXT("composter"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.9), 6, 0.3f, FVector::ZeroVector, FColor(120, 180, 60));
			return true;
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if ((FMCBlocks::MetaOf(S) & 15) == 7) { W.SetState(P, WithMeta(S, 8), MCSet_Render); W.PlaySound(TEXT("composter_ready"), P.Center() * MC::InvBlockSize); }
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return FMCBlocks::MetaOf(S) & 15; }
	};

	class FLecternBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer())); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const bool bBook = MCMeta::Bit(FMCBlocks::MetaOf(S), 2);
			if (!bBook && (HeldIs(Player, TEXT("writable_book")) || HeldIs(Player, TEXT("written_book")) || HeldIs(Player, TEXT("book"))))
			{
				W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) | 4)), MCSet_Render);
				if (!Player->IsCreative()) Player->ConsumeHeld(1);
				W.PlaySound(TEXT("book_put"), P.Center() * MC::InvBlockSize);
				return true;
			}
			if (bBook)
			{
				// turning pages emits a short redstone pulse
				W.PlaySound(TEXT("book_page_turn"), P.Center() * MC::InvBlockSize);
				if (Player->bSneaking)
				{
					W.SetState(P, WithMeta(S, (uint8)(FMCBlocks::MetaOf(S) & ~4)), MCSet_Render);
					Player->GiveItem(FMCItemStack::Of(TEXT("book"), 1));
				}
				return true;
			}
			return false;
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 2) ? 1 : 0; }
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("lectern"), 1));
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 2)) Out.Add(FMCItemStack::Of(TEXT("book"), 1));
			return true;
		}
	};

	class FBookshelfBeh : public FMCBlockBehavior
	{
	public:
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (Tool && Tool->GetEnchant(EMCEnchant::SilkTouch) > 0) { Out.Add(FMCItemStack::Of(FMCBlocks::GetByState(S).Name, 1)); return true; }
			Out.Add(FMCItemStack::Of(TEXT("book"), 3));
			return true;
		}
	};

	/** Crafter: 3x3 grid that crafts one item on a rising redstone edge and ejects it forward. */
	class FCrafterBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State((uint8)MC::Opposite(LookFacing6(C)) & 3); }
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCContainerEntity>(EMCBlockEntityType::Crafter, 9); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			static TSet<FMCBlockPos> Powered;
			if (bPowered && !Powered.Contains(P)) { Powered.Add(P); W.ScheduleTick(P, FMCBlocks::BlockOf(S), 4); }
			else if (!bPowered) Powered.Remove(P);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			FMCContainer* C = BE ? BE->GetContainer() : nullptr;
			if (!C || C->Num() < 9) return;
			FMCItemStack Result;
			const FMCRecipe* R = FMCRecipes::FindCrafting(C->Slots.GetData(), 3, 3, Result);
			if (!R || Result.IsEmpty()) { W.PlaySound(TEXT("crafter_fail"), P.Center() * MC::InvBlockSize); return; }
			for (int32 i = 0; i < 9; ++i) if (!(*C)[i].IsEmpty()) { (*C)[i].Count -= 1; if ((*C)[i].Count <= 0) (*C)[i].Clear(); }
			const EMCFace F = MC::HorizontalFace(FMCBlocks::MetaOf(S) & 3);
			const FIntVector& D = MC::FaceDir[(int32)F];
			W.SpawnItem(P.Center() * MC::InvBlockSize + FVector(D.X, D.Y, D.Z) * 0.7, Result, false, 0.2f);
			W.PlaySound(TEXT("crafter_craft"), P.Center() * MC::InvBlockSize);
			W.MarkModified(P);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			if (!BE || !BE->GetContainer()) return 0;
			int32 N = 0;
			for (const FMCItemStack& I : BE->GetContainer()->Slots) if (!I.IsEmpty()) ++N;
			return N;
		}
	};

	/** Shelf: holds three items displayed on the front; right-click swaps the held item with the targeted slot. */
	class FShelfBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer())); }
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCContainerEntity>(EMCBlockEntityType::Shelf, 3); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			FMCContainer* C = BE ? BE->GetContainer() : nullptr;
			if (!C) return false;
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			// slot from the hit position along the shelf's width
			const FIntVector& R = MC::FaceDir[(int32)MC::RotateY(F, 1)];
			const double Along = (Hit.X - 0.5) * R.X + (Hit.Y - 0.5) * R.Y + 0.5;
			const int32 Slot = FMath::Clamp((int32)(Along * 3.0), 0, 2);
			FMCItemStack Tmp = (*C)[Slot];
			(*C)[Slot] = Player->HeldConst().Copy();
			Player->Held() = Tmp;
			W.PlaySound(TEXT("shelf_place"), P.Center() * MC::InvBlockSize);
			W.MarkModified(P);
			W.MarkBlockDirty(P);
			return true;
		}
	};
}

void MCBeh::RegisterFunctional()
{
	Register(EMCBeh::CraftingTable, new FStationBeh(FStationBeh::Crafting, true));
	Register(EMCBeh::EnchantingTable, new FStationBeh(FStationBeh::Enchanting));
	Register(EMCBeh::Anvil, new FStationBeh(FStationBeh::Anvil));
	Register(EMCBeh::SmithingTable, new FStationBeh(FStationBeh::Smithing));
	Register(EMCBeh::Stonecutter, new FStationBeh(FStationBeh::Stonecutter, true));
	Register(EMCBeh::Loom, new FStationBeh(FStationBeh::Loom, true));
	Register(EMCBeh::Grindstone, new FStationBeh(FStationBeh::Grindstone));
	Register(EMCBeh::CartographyTable, new FStationBeh(FStationBeh::Cartography));
	Register(EMCBeh::Furnace, new FFurnaceBeh(EMCFurnaceKind::Furnace));
	Register(EMCBeh::BlastFurnace, new FFurnaceBeh(EMCFurnaceKind::Blast));
	Register(EMCBeh::Smoker, new FFurnaceBeh(EMCFurnaceKind::Smoker));
	FChestBeh* Chest = new FChestBeh(); Chest->bTrapped = false;
	FChestBeh* Trapped = new FChestBeh(); Trapped->bTrapped = true;
	FChestBeh* Copper = new FChestBeh(); Copper->bTrapped = false;
	Register(EMCBeh::Chest, Chest);
	Register(EMCBeh::TrappedChest, Trapped);
	Register(EMCBeh::CopperChest, Copper);
	Register(EMCBeh::EnderChest, new FEnderChestBeh());
	Register(EMCBeh::Barrel, new FBarrelBeh());
	Register(EMCBeh::ShulkerBox, new FShulkerBoxBeh());
	Register(EMCBeh::BrewingStand, new FBrewingBeh());
	Register(EMCBeh::Bed, new FBedBeh());
	Register(EMCBeh::Campfire, new FCampfireBeh());
	Register(EMCBeh::Cauldron, new FCauldronBeh());
	Register(EMCBeh::Beacon, new FBeaconBeh());
	Register(EMCBeh::RespawnAnchor, new FRespawnAnchorBeh());
	Register(EMCBeh::Jukebox, new FJukeboxBeh());
	Register(EMCBeh::Spawner, new FSpawnerBeh());
	Register(EMCBeh::Bell, new FBellBeh());
	Register(EMCBeh::Cake, new FCakeBeh());
	Register(EMCBeh::Candle, new FCandleBeh());
	Register(EMCBeh::Composter, new FComposterBeh());
	Register(EMCBeh::Lectern, new FLecternBeh());
	Register(EMCBeh::Bookshelf, new FBookshelfBeh());
	Register(EMCBeh::ChiseledBookshelf, MCBehaviors::Get(EMCBeh::FacingToPlayer));
	Register(EMCBeh::Crafter, new FCrafterBeh());
	Register(EMCBeh::Shelf, new FShelfBeh());
}
