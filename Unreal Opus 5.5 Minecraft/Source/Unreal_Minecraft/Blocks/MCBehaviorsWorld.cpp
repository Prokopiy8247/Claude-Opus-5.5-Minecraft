// World behaviours: fluids, falling blocks, fire, portals, ice/snow, physics-ish blocks, drips, amethyst,
// sulfur, creaking hearts, scaffolding, dragon egg, bedrock.
#include "Blocks/MCBehaviorsInternal.h"
#include "Gen/MCBiomes.h"
#include "Game/MCGame.h"

using namespace MCBeh;

namespace MCBehPlants
{
	int32 LightAt(const FMCWorld& W, const FMCBlockPos& P);
	int32 ToolFortune(const FMCItemStack* Tool);
	bool ToolIs(const FMCItemStack* Tool, const TCHAR* Name);
}
using namespace MCBehPlants;

namespace
{
	const EMCFace GHoriz[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
	FORCEINLINE FMCBlockId WaterId() { return FMCBlocks::C.WaterId; }
	FORCEINLINE FMCBlockId LavaId() { return FMCBlocks::C.LavaId; }
	FORCEINLINE bool IsWater(FMCState S) { return FMCBlocks::BlockOf(S) == FMCBlocks::C.WaterId; }
	FORCEINLINE bool IsLava(FMCState S) { return FMCBlocks::BlockOf(S) == FMCBlocks::C.LavaId; }
	FORCEINLINE bool IsFluid(FMCState S) { return IsWater(S) || IsLava(S); }
	FORCEINLINE int32 LevelOf(FMCState S) { return FMCBlocks::MetaOf(S) & 15; }
	FORCEINLINE bool IsSource(FMCState S) { return LevelOf(S) == 0; }
	FORCEINLINE int32 EffectiveLevel(FMCState S) { const int32 L = LevelOf(S); return L >= 8 ? 0 : L; }

	FORCEINLINE FMCBlockPos FPos(const FMCBlockPos& P, int32 F) { return P.Offset((EMCFace)F); }

	/** Fluids are opaque to each other's flow but water can flow into water. */
	bool CanFlowInto(const FMCWorld& W, const FMCBlockPos& P, bool bLava)
	{
		const FMCState S = W.GetState(P);
		if (S == 0) return true;
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (IsWater(S) || IsLava(S)) return false;
		if (B.Behavior->IsFluidReplaceable(S)) return true;
		if (B.Has(MCB_Replaceable)) return true;
		if (B.Has(MCB_Plant) && !B.Has(MCB_Solid)) return true;
		if (B.Model == EMCModel::SnowLayer) return true;
		return false;
	}
}

namespace MCBehFluids
{
	/** Spread distance (Minecraft: water 7, lava 3 in the overworld). */
	int32 SpreadDistance(const FMCWorld& W, bool bLava)
	{
		if (bLava) return W.Dim == EMCDimension::Nether ? 7 : 3;
		return W.Dim == EMCDimension::Nether ? 7 : 7;
	}

	/** Recompute the level of the fluid at P from its neighbours and schedule flow. */
	void UpdateFluid(FMCWorld& W, const FMCBlockPos& P, FMCState S);
	/** Try to place fluid into a neighbour cell. */
	void FlowInto(FMCWorld& W, const FMCBlockPos& From, const FMCBlockPos& To, FMCState S, int32 NewLevel, bool bDown);
}

namespace
{
	// ================================================================================================================
	/** Water & lava flow with Minecraft-like spreading, source duplication and interaction. */
	class FFluidBeh : public FMCBlockBehavior
	{
	public:
		bool bLava = false;
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(0); }
		virtual bool IsFluidReplaceable(FMCState S) const override { return false; }
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Schedule(W, P, S); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (From == P) return;
			Schedule(W, P, S);
			// lava + water interaction
			if (bLava)
			{
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F);
					if (!IsWater(W.GetState(Q))) continue;
					Interact(W, P, Q, (EMCFace)F);
					return;
				}
			}
			else
			{
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F);
					if (!IsLava(W.GetState(Q))) continue;
					FMCBlocks::GetByState(W.GetState(Q)).Behavior->OnNeighborChanged(W, Q, W.GetState(Q), P);
					return;
				}
			}
		}
		void Interact(FMCWorld& W, const FMCBlockPos& LavaPos, const FMCBlockPos& WaterPos, EMCFace Dir) const
		{
			const FMCState LavaS = W.GetState(LavaPos);
			const FMCState WaterS = W.GetState(WaterPos);
			const bool bSourceLava = IsSource(LavaS);
			const bool bSourceWater = IsSource(WaterS);
			// the smaller body is converted
			const FMCBlock* Result = nullptr;
			if (bSourceLava)
			{
				// source lava touched by water: obsidian if the water is above, or if water flowed onto it
				Result = FMCBlocks::Find(TEXT("obsidian"));
				W.PlaySound(TEXT("lava_extinguish"), LavaPos.Center() * MC::InvBlockSize, 0.5f, 2.6f);
				W.SpawnParticles(TEXT("smoke"), LavaPos.Center() * MC::InvBlockSize + FVector(0, 0, 0.5), 8, 0.3f);
				if (Dir == EMCFace::Up) { /* water above lava */ }
			}
			else
			{
				Result = Dir == EMCFace::Up ? FMCBlocks::Find(TEXT("cobblestone")) : FMCBlocks::Find(TEXT("stone"));
				W.PlaySound(TEXT("lava_extinguish"), LavaPos.Center() * MC::InvBlockSize, 0.5f, 2.6f);
			}
			if (Result) W.SetState(LavaPos, Result->BaseState);
		}
		static void Schedule(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			if (W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) return;
			W.ScheduleTick(P, FMCBlocks::BlockOf(S), LevelOf(S) >= 8 ? MCBeh::FluidTickDelay(W, IsLava(S)) : MCBeh::FluidTickDelay(W, IsLava(S)) + 5);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			MCBehFluids::UpdateFluid(W, P, S);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			// lava ignites flammable neighbours
			if (bLava)
			{
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F);
					const FMCState QS = W.GetState(Q);
					if (QS == 0) continue;
					const FMCBlock& QB = FMCBlocks::GetByState(QS);
					if (QB.FireBurn > 0 && R.NextInt(200) < QB.FireBurn) W.SetState(Q, FMCBlocks::C.Fire);
				}
			}
			// water hydrates farmland below (handled by farmland itself) and puts out fire
			for (int32 F = 0; F < 6; ++F)
			{
				const FMCBlockPos Q = P.Offset((EMCFace)F);
				const FMCState QS = W.GetState(Q);
				if (QS == 0) continue;
				const FMCBlock& QB = FMCBlocks::GetByState(QS);
				if (QB.Model == EMCModel::Fire) W.DestroyBlock(Q, false, nullptr, nullptr, true);
			}
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override { return true; }
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return bLava ? 15 : 0; }
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || E->bFireImmune) return;
			if (bLava)
			{
				E->SetOnFire(15);
				if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("lava")), 4.f);
			}
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (bLava && R.Chance(0.05)) W.SpawnParticles(TEXT("lava_pop"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + 0.9f), 1, 0.f, FVector(0, 0, 0.4f), FColor(255, 160, 40));
		}
	};
}

namespace MCBehFluids
{
	void FlowInto(FMCWorld& W, const FMCBlockPos& From, const FMCBlockPos& To, FMCState S, int32 NewLevel, bool bDown)
	{
		const bool bLava = IsLava(S);
		const FMCState Cur = W.GetState(To);
		if (IsFluid(Cur))
		{
			// merge only when we are stronger
			const int32 CurLvl = EffectiveLevel(Cur);
			const int32 MyLvl = bDown ? 0 : NewLevel;
			if (IsLava(Cur) == bLava && CurLvl <= MyLvl) return;
			if (IsLava(Cur) != bLava) return;
		}
		if (Cur != 0 && !CanFlowInto(W, To, bLava)) return;
		if (Cur != 0 && !IsFluid(Cur))
		{
			W.DestroyBlock(To, true);
		}
		W.SetState(To, FMCBlocks::GetByState(S).State((uint8)NewLevel), MCSet_Render | MCSet_Light | MCSet_Neighbors);
		FFluidBeh::Schedule(W, To, W.GetState(To));
	}

	void UpdateFluid(FMCWorld& W, const FMCBlockPos& P, FMCState S)
	{
		const bool bLava = IsLava(S);
		const FMCBlock& B = FMCBlocks::GetByState(S);
		const int32 MaxSpread = SpreadDistance(W, bLava);
		const bool bSource = IsSource(S);
		if (!bSource)
		{
			// recompute the level from the strongest neighbour
			int32 Best = 8;
			bool bDrop = false;
			for (int32 F = 0; F < 6; ++F)
			{
				const FMCBlockPos Q = P.Offset((EMCFace)F);
				const FMCState QS = W.GetState(Q);
				if (!IsFluid(QS) || IsLava(QS) != bLava) continue;
				if (F == (int32)EMCFace::Up) { bDrop = true; continue; }
				if (F == (int32)EMCFace::Down) continue;
				const int32 L = EffectiveLevel(QS) + 1;
				if (IsSource(QS)) Best = FMath::Min(Best, 1);
				else Best = FMath::Min(Best, L);
			}
			// infinite water: a flowing cell between two or more horizontal sources, resting on something solid
			// (or on a source), becomes a source itself. Sources never convert their neighbours, so still pools
			// stay the size they were generated at instead of creeping across every floor they touch.
			if (!bLava)
			{
				int32 Sources = 0;
				for (int32 F = 2; F < 6; ++F)
				{
					const FMCState QS = W.GetState(P.Offset((EMCFace)F));
					if (IsWater(QS) && IsSource(QS)) ++Sources;
				}
				const FMCState BelowS = W.GetState(P.Down());
				if (Sources >= 2 && (MCBehaviorUtil::IsFaceSturdy(W, P.Down(), EMCFace::Up) || (IsWater(BelowS) && IsSource(BelowS))))
				{
					W.SetState(P, B.State(0), MCSet_Render);
					FFluidBeh::Schedule(W, P, W.GetState(P));
					return;
				}
			}
			const FMCState Above = W.GetState(P.Up());
			if (IsFluid(Above) && IsLava(Above) == bLava)
			{
				// falling fluid
				if (LevelOf(S) < 8) W.SetState(P, B.State(8), MCSet_Render);
				Best = 0;
			}
			else if (Best >= 8 || Best > MaxSpread)
			{
				// no supply: recede
				W.SetState(P, 0, MCSet_Default);
				for (int32 F = 0; F < 6; ++F) W.NotifyNeighbors(P.Offset((EMCFace)F));
				return;
			}
			else if (EffectiveLevel(S) != Best)
			{
				W.SetState(P, B.State((uint8)Best), MCSet_Render);
				S = W.GetState(P);
			}
		}
		const int32 MyLevel = EffectiveLevel(W.GetState(P));
		// flow down first
		if (CanFlowInto(W, P.Down(), bLava))
		{
			FlowInto(W, P, P.Down(), B.State(0), 8, true);
			return;
		}
		if (MyLevel >= MaxSpread && !IsSource(W.GetState(P))) return;
		const int32 Next = IsSource(W.GetState(P)) ? 1 : MyLevel + 1;
		if (Next > MaxSpread) return;
		// prefer the directions with a drop below
		TArray<int32> Prefer;
		for (int32 i = 0; i < 4; ++i)
		{
			const FMCBlockPos Q = P.Offset(GHoriz[i]);
			if (!CanFlowInto(W, Q, bLava)) continue;
			if (CanFlowInto(W, Q.Down(), bLava)) Prefer.Add(i);
		}
		if (Prefer.Num() == 0)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const FMCBlockPos Q = P.Offset(GHoriz[i]);
				if (!CanFlowInto(W, Q, bLava)) continue;
				FlowInto(W, P, Q, B.State(0), Next, false);
			}
		}
		else
		{
			for (int32 i : Prefer) FlowInto(W, P, P.Offset(GHoriz[i]), B.State(0), Next, false);
		}
	}
}

namespace
{
	// ================================================================================================================
	/** Sand / gravel / concrete powder falling. */
	class FFallingBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Check(W, P, S); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (From == P.Down()) Check(W, P, S);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { Check(W, P, S); }
		void Check(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			const FMCBlockPos Below = P.Down();
			const FMCState BS = W.GetState(Below);
			const bool bBlocked = BS != 0 && !FMCBlocks::IsFluid(BS) && !(FMCBlocks::Info(BS).Flags & MCB_Replaceable) && FMCBlocks::GetByState(BS).Model != EMCModel::Fire;
			if (bBlocked) return;
			// pistons and other full blocks under are handled by bBlocked; falling starts
			if (!W.Game) return;
			W.SetState(P, 0, MCSet_Render | MCSet_Light);
			if (AMCFallingBlock* FB = W.Game->SpawnEntity<AMCFallingBlock>(&W, FVector(P.X + 0.5, P.Y + 0.5, P.Z)))
			{
				FB->State = S;
				if (FMCBlocks::GetByState(S).Name == TEXT("anvil") || FMCBlocks::GetByState(S).Variant == TEXT("anvil")) FB->bHurtEntities = true;
				FB->InitEntity();
			}
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("gravel"))
			{
				const int32 Fortune = ToolFortune(Tool);
				float Chance = 0.1f + Fortune * 0.02f + (Tool && Tool->GetEnchant(EMCEnchant::SilkTouch) > 0 ? 1.f : 0.f);
				if (Fortune >= 3) Chance = 1.f;
				if (R.NextFloat() < Chance) Out.Add(FMCItemStack::Of(TEXT("flint"), 1));
				return true;
			}
			return false; // default (self)
		}
	};

	class FConcretePowderBeh : public FFallingBeh
	{
	public:
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			bool bWater = false;
			for (int32 F = 0; F < 6 && !bWater; ++F) if (MCBehaviorUtil::IsWaterAt(W, P.Offset((EMCFace)F))) bWater = true;
			if (bWater)
			{
				const FName N = FMCBlocks::GetByState(S).Name;
				const FString Base = N.ToString().Replace(TEXT("_concrete_powder"), TEXT("_concrete"));
				const FMCState Conc = StateOf(*Base);
				W.PlaySound(TEXT("concrete_harden"), P.Center() * MC::InvBlockSize);
				W.SetState(P, Conc ? Conc : S, MCSet_Default);
				return;
			}
			FFallingBeh::OnNeighborChanged(W, P, S, From);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			OnNeighborChanged(W, P, S, P);
		}
	};

	// ================================================================================================================
	class FIceBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name != TEXT("ice")) return;
			if (LightAt(W, P) > 11 - FMCBlocks::Info(S).Opacity && !HasAdjacentWater(W, P)) W.SetState(P, FMCBlocks::C.Water);
		}
		static bool HasAdjacentWater(const FMCWorld& W, const FMCBlockPos& P)
		{
			for (int32 F = 0; F < 6; ++F) if (IsWater(W.GetState(P.Offset((EMCFace)F)))) return true;
			return false;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			// frost walker: handled in the player tick
		}
		virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const override {}
	};

	/** Snow layers: stack up to 8, melt from light, form on cold ground. */
	class FSnowLayerBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			if (Below == 0) return false;
			const FMCBlock& BB = FMCBlocks::GetByState(Below);
			if (BB.Name == TEXT("snow")) return (FMCBlocks::MetaOf(Below) & 7) == 7;
			return MCBehaviorUtil::IsFaceSturdy(W, P.Down(), EMCFace::Up) || (FMCBlocks::Info(Below).Flags & MCB_Leaves) != 0;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (LightAt(W, P) > 11)
			{
				const int32 Layers = (FMCBlocks::MetaOf(S) & 7) + 1;
				if (Layers > 1) W.SetState(P, WithMeta(S, (uint8)(Layers - 2)), MCSet_Render | MCSet_Light);
				else W.SetState(P, 0, MCSet_Render | MCSet_Light);
				return;
			}
			// snow placed by the player accumulates when it snows
			if (W.Dim != EMCDimension::Overworld || !S) return;
			if (!MCBeh::IsRainingAbove(W, P) || !W.Game || !W.Game->bRaining) return;
			const uint8 Biome = W.GetBiome(P);
			if (!FMCBiomes::Get(Biome).bSnowy) return;
			const int32 Layers = (FMCBlocks::MetaOf(S) & 7) + 1;
			if (Layers < 8 && R.Chance(0.05)) W.SetState(P, WithMeta(S, (uint8)Layers), MCSet_Render);
		}
		virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override {}
	};

	// ================================================================================================================
	/** Fire: spreads, burns out, infinite on netherrack/soul soil/magma. */
	class FFireBeh : public FMCBlockBehavior
	{
	public:
		static bool IsSoul(FMCState S) { return FMCBlocks::GetByState(S).Name == TEXT("soul_fire"); }
		static int32 BurnOdds(FMCState Below)
		{
			const FMCBlock& B = FMCBlocks::GetByState(Below);
			if (B.Name == TEXT("netherrack") || B.Name == TEXT("magma_block") || B.Name == TEXT("soul_soil")) return 0; // infinite
			if (B.Name == TEXT("crimson_nylium") || B.Name == TEXT("warped_nylium")) return 0;
			return (int32)B.FireBurn;
		}
		static int32 SpreadOdds(FMCState Below)
		{
			const FMCBlock& B = FMCBlocks::GetByState(Below);
			if (B.Name == TEXT("netherrack") || B.Name == TEXT("magma_block") || B.Name == TEXT("soul_soil")) return 0;
			return (int32)B.FireSpread;
		}
		static bool CanBurnAt(FMCWorld& W, const FMCBlockPos& P)
		{
			if (W.GetState(P) != 0) return false;
			const FMCState Below = W.GetState(P.Down());
			if (Below == 0) return false;
			const FMCBlock& B = FMCBlocks::GetByState(Below);
			if (B.Has(MCB_Flammable) || B.FireBurn > 0) return true;
			return B.Name == TEXT("netherrack") || B.Name == TEXT("magma_block") || B.Name == TEXT("soul_soil");
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			const FMCBlockPos Below = C.Pos.Down();
			const FMCState BS = C.World->GetState(Below);
			if (FMCBlocks::GetByState(BS).Name == TEXT("soul_soil") || FMCBlocks::GetByState(BS).Name == TEXT("soul_sand")) return StateOf(TEXT("soul_fire"));
			if (!CanBurnAt(*C.World, C.Pos)) return 0;
			return B.State(C.World->Rand.NextInt(16));
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (IsSoul(S)) { const FName BN = FMCBlocks::GetByState(W.GetState(P.Down())).Name; return BN == TEXT("soul_soil") || BN == TEXT("soul_sand"); }
			return CanBurnAt(W, P);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.DestroyBlock(P, false);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (!W.Game || !W.Game->Rules.bDoFireTick) return;
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) { W.SetState(P, 0, MCSet_Render | MCSet_Light); return; }
			const FMCState Below = W.GetState(P.Down());
			const int32 Burn = BurnOdds(Below);
			const bool bInfinite = Burn == 0 && (FMCBlocks::GetByState(Below).Has(MCB_Flammable) == false);
			if (!bInfinite && R.NextInt(Burn + 1) == 0) { W.SetState(P, 0, MCSet_Render | MCSet_Light); return; }
			// spread
			const int32 Spread = SpreadOdds(Below);
			int32 Age = FMCBlocks::MetaOf(S) & 15;
			if (Age < 15 && R.NextInt(4) == 0) { W.SetState(P, WithMeta(S, (uint8)(Age + 1)), MCSet_Render); Age = (Age + 1); }
			if (Age < 15) return;
			if (R.NextInt(Spread + 1) != 0) return;
			// ignite neighbours
			for (int32 i = 0; i < 8; ++i)
			{
				const FMCBlockPos Q(P.X + R.Range(-1, 1), P.Y + R.Range(-1, 1), P.Z + R.Range(-1, R.Chance(0.5) ? 1 : 0));
				if (Q == P) continue;
				const FMCState QS = W.GetState(Q);
				if (QS != 0)
				{
					// burn the block away, leaving fire behind
					const FMCBlock& QB = FMCBlocks::GetByState(QS);
					if (QB.FireBurn > 0 && R.NextInt(200) < QB.FireBurn)
					{
						const FMCBlockPos FirePos = Q.Down();
						if (CanBurnAt(W, FirePos) && W.GetState(FirePos) == 0) W.SetState(FirePos, MCBeh::StateOf(TEXT("fire"), (uint8)R.NextInt(16)));
					}
					continue;
				}
				if (CanBurnAt(W, Q)) W.SetState(Q, MCBeh::StateOf(TEXT("fire"), (uint8)(R.NextInt(8) + (Age > 12 ? 8 : 0))));
			}
			// TNT, campfires, candles etc. react to fire (handled by their own random ticks / neighbours)
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			E->SetOnFire(8);
			if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("fire")), 1.f);
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.35))
				W.SpawnParticles(IsSoul(S) ? TEXT("soul_fire_flame") : TEXT("flame"), FVector(P.X + R.FRange(0.2f, 0.8f), P.Y + R.FRange(0.2f, 0.8f), P.Z + R.FRange(0.2f, 0.9f)), 1, 0.f, FVector(0, 0, 0.01f), FColor::White);
		}
	};

	// ================================================================================================================
	class FNetherPortalBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const bool bAxisY = MCMeta::Bit(FMCBlocks::MetaOf(S), 0); // true: portal spans X (frame along X)
			const EMCFace L = bAxisY ? EMCFace::West : EMCFace::North;
			const EMCFace R = bAxisY ? EMCFace::East : EMCFace::South;
			const FMCState LS = W.GetState(P.Offset(L)), RS = W.GetState(P.Offset(R));
			if ((LS == 0 || FMCBlocks::IsReplaceable(LS)) && (RS == 0 || FMCBlocks::IsReplaceable(RS))) return false;
			if (!(FMCBlocks::Info(LS).Flags & (MCB_Opaque | MCB_Unbreakable)) && LS != 0) return false;
			return true;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.DestroyBlock(P, false);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (Player && MCBeh::HeldIs(Player, TEXT("flint_and_steel")))
			{
				// light the portal: find the other dimension coordinates
				Player->GiveXP(0);
				return false;
			}
			return false;
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override { return true; }
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || E->PortalCooldown > 0 || E->bNoPhysics) return;
			++E->PortalTime;
			if (E->PortalTime >= 80)
			{
				E->PortalTime = 0;
				E->PortalCooldown = 300;
				if (W.Game) W.Game->ChangeDimension(E, W.Dim == EMCDimension::Nether ? EMCDimension::Overworld : EMCDimension::Nether);
			}
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { W.AddPortalPOI(P); }
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.4)) W.SpawnParticles(TEXT("portal"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + R.NextFloat()), 1, 0.f, FVector::ZeroVector, FColor(150, 60, 220));
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return 11; }
	};

	// ================================================================================================================
	class FEndPortalBeh : public FMCBlockBehavior
	{
	public:
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override { return true; }
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			// as in Minecraft, touching any end portal block travels at once: into the End, or home from its exit portal
			if (!E || E->PortalCooldown > 0 || !W.Game) return;
			W.Game->OnEndPortalEntered(E);
			E->PortalCooldown = 300;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.5)) W.SpawnParticles(TEXT("end_rod"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + R.NextFloat()), 1, 0.f, FVector(0, 0, 0.01f), FColor(120, 60, 220));
		}
	};

	class FEndPortalFrameBeh : public FMCBlockBehavior
	{
	public:
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const uint8 M = FMCBlocks::MetaOf(S);
			if (MCMeta::Bit(M, 2)) return false;
			const FMCItem* I = MCBeh::HeldItem(Player);
			if (!I || I->Name != FName(TEXT("ender_eye"))) return false;
			W.SetState(P, WithMeta(S, (uint8)(M | 4)), MCSet_Render);
			W.PlaySound(TEXT("end_portal_frame_fill"), P.Center() * MC::InvBlockSize, 0.5f, 1.f);
			if (!Player->IsCreative()) Player->ConsumeHeld(1);
			TryActivate(W, P);
			return true;
		}
		/** All 12 frames filled -> the 3x3 inside becomes the portal, at the frames' own height (as in Minecraft). */
		static void TryActivate(FMCWorld& W, const FMCBlockPos& AnyFrame)
		{
			// the ring's centre is at most 2 blocks from any of its frames
			for (int32 ox = -2; ox <= 2; ++ox)
				for (int32 oy = -2; oy <= 2; ++oy)
				{
					const FMCBlockPos C(AnyFrame.X + ox, AnyFrame.Y + oy, AnyFrame.Z);
					if (!IsComplete(W, C)) continue;
					const FMCState Portal = MCBeh::StateOf(TEXT("end_portal"));
					for (int32 dx = -1; dx <= 1; ++dx) for (int32 dy = -1; dy <= 1; ++dy) W.SetState(FMCBlockPos(C.X + dx, C.Y + dy, C.Z), Portal);
					W.PlaySound(TEXT("end_portal_spawn"), FVector(C.X + 0.5, C.Y + 0.5, C.Z + 0.5), 1.f, 1.f);
					W.SpawnParticles(TEXT("end_rod"), FVector(C.X + 0.5, C.Y + 0.5, C.Z + 0.5), 40, 2.f, FVector::ZeroVector, FColor(120, 60, 220));
					return;
				}
		}
		static bool IsFrame(const FMCWorld& W, const FMCBlockPos& P, bool bNeedEye)
		{
			const FMCState S = W.GetState(P);
			if (FMCBlocks::GetByState(S).Name != TEXT("end_portal_frame")) return false;
			if (bNeedEye && !MCMeta::Bit(FMCBlocks::MetaOf(S), 2)) return false;
			return true;
		}
		/** 12 frames with eyes around a 3x3 interior, all at the centre's height; corners and interior can be anything. */
		static bool IsComplete(const FMCWorld& W, const FMCBlockPos& Center)
		{
			for (int32 i = -1; i <= 1; ++i)
			{
				if (!IsFrame(W, FMCBlockPos(Center.X + i, Center.Y - 2, Center.Z), true)) return false;
				if (!IsFrame(W, FMCBlockPos(Center.X + i, Center.Y + 2, Center.Z), true)) return false;
				if (!IsFrame(W, FMCBlockPos(Center.X - 2, Center.Y + i, Center.Z), true)) return false;
				if (!IsFrame(W, FMCBlockPos(Center.X + 2, Center.Y + i, Center.Z), true)) return false;
			}
			return true;
		}
	};

	class FEndGatewayBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCEndGatewayEntity>(); }
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || E->PortalCooldown > 0 || !W.Game) return;
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			FMCEndGatewayEntity* GW = static_cast<FMCEndGatewayEntity*>(BE);
			if (!GW) return;
			if (GW->Cooldown > 0) return;
			GW->Cooldown = 40;
			if (!GW->bHasExit)
			{
				// pick an exit near the outer islands (or back to the main island)
				FMCRandom& R = W.Rand;
				const double Dist = W.Dim == EMCDimension::End ? 1000.0 : 96.0;
				const double A = R.NextDouble() * 2.0 * PI;
				const int32 EX = (int32)(FMath::Cos(A) * Dist), EY = (int32)(FMath::Sin(A) * Dist);
				GW->Exit = FMCBlockPos(EX, EY, 75);
				GW->bHasExit = true;
			}
			const FVector Target(GW->Exit.X + 0.5, GW->Exit.Y + 0.5, GW->Exit.Z);
			W.PlaySound(TEXT("end_gateway_travel"), P.Center() * MC::InvBlockSize);
			W.SpawnParticles(TEXT("portal"), P.Center() * MC::InvBlockSize, 20, 0.5f, FVector::ZeroVector, FColor(200, 100, 255));
			E->TeleportTo(Target);
			E->PortalCooldown = 40;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.6)) W.SpawnParticles(TEXT("portal"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 1, 0.3f, FVector::ZeroVector, FColor(220, 120, 255));
		}
	};

	// ================================================================================================================
	class FMagmaBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || E->bFireImmune) return;
			// standing on top burns, standing inside hurts more
			const bool bOnTop = E->Pos.Z >= P.Z + 1.0 - 0.05;
			if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("hot_floor")), bOnTop ? 1.f : 2.f);
			E->SetOnFire(bOnTop ? 1 : 4);
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.1)) W.SpawnParticles(TEXT("lava_pop"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + 1.05f), 1, 0.f, FVector(0, 0, 0.2f), FColor(255, 140, 40));
		}
		virtual bool IsPowerSource(FMCState S) const override { return false; }
	};

	class FSoulSandBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || E->Kind == EMCEntityKind::Player) return;
			const FIntVector& D = MC::FaceDir[(int32)MC::Opposite(MC::FaceFromYaw(E->Yaw))];
			E->Vel.X += D.X * 0.004;
			E->Vel.Y += D.Y * 0.004;
		}
		virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override {}
	};

	class FHoneyBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			E->Vel.Z *= 0.3;
			E->Vel.X *= 0.4; E->Vel.Y *= 0.4;
			// entities slide down the side of honey blocks
			const bool bBelow = E->Pos.Z + E->Height * 0.5 < P.Z + 0.5;
			if (bBelow)
			{
				E->Vel.Z = FMath::Max(E->Vel.Z, -0.1);
				E->FallDistance = 0.f;
			}
		}
		virtual float FallDamageMultiplier(FMCState S) const override { return 0.2f; }
	};

	class FSlimeBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const override
		{
			if (!E || Distance < 0.2f) return;
			if (Distance < 1.5f) return;
			const FVector V = E->Vel;
			FVector New = FVector(V.X, V.Y, -V.Z) * 0.8;
			if (New.Z < 0.1 && !E->bSneaking) New.Z = 0.1;
			E->Vel = New;
			E->FallDistance = 0.f;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			// slime blocks drag entities standing on their neighbours when pushed (simplified)
		}
		virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override {}
	};

	class FCobwebBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			E->StuckSpeedMultiplier = FVector(0.25, 0.25, 0.05);
			E->FallDistance = 0.f;
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const int32 Fortune = ToolFortune(Tool);
			Out.Add(FMCItemStack::Of(TEXT("string"), FMath::Max(1, 1 + R.NextInt(2) + Fortune)));
			return true;
		}
	};

	class FPowderSnowBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			E->Vel.X *= 0.9; E->Vel.Y *= 0.9;
			E->Vel.Z = FMath::Max(E->Vel.Z, -0.05);
			E->FallDistance = 0.f;
			if (E->IsLiving())
			{
				AMCLiving* L = static_cast<AMCLiving*>(E);
				if (L->FreezeTicks < 300) L->FreezeTicks += 1;
			}
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (ToolIs(Tool, TEXT("bucket")) || (Tool && Tool->Item().Name == FName(TEXT("bucket")))) Out.Add(FMCItemStack::Of(TEXT("powder_snow_bucket"), 1));
			return true;
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override {}
	};

	class FSpongeBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Absorb(W, P); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override { Absorb(W, P); }
		static void Absorb(FMCWorld& W, const FMCBlockPos& P)
		{
			const FMCBlock& B = FMCBlocks::GetByState(W.GetState(P));
			if (B.Name != TEXT("sponge")) return;
			// breadth-first search through water, up to 64 blocks
			TArray<FMCBlockPos> Queue;
			TSet<FMCBlockPos> Seen;
			Queue.Add(P);
			Seen.Add(P);
			int32 Absorbed = 0;
			for (int32 i = 0; i < Queue.Num() && Absorbed < 64; ++i)
			{
				const FMCBlockPos C = Queue[i];
				if (C != P && FMCBlocks::GetByState(W.GetState(C)).Name == TEXT("wet_sponge")) break; // adjacent sponge also absorbs
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = C.Offset((EMCFace)F);
					if (Seen.Contains(Q)) continue;
					Seen.Add(Q);
					const FMCState QS = W.GetState(Q);
					if (IsWater(QS))
					{
						W.SetState(Q, 0, MCSet_Render | MCSet_Light);
						++Absorbed;
						Queue.Add(Q);
						continue;
					}
					// evaporate waterlogged plants
					if (FMCBlocks::Info(QS).Flags & MCB_Waterlogged)
					{
						W.SetState(Q, 0, MCSet_Render | MCSet_Light);
						++Absorbed;
						continue;
					}
					if (QS == 0 && Seen.Num() < 1024) Queue.Add(Q);
				}
			}
			if (Absorbed > 0)
			{
				W.SetState(P, StateOf(TEXT("wet_sponge")));
				W.PlaySound(TEXT("sponge_absorb"), P.Center() * MC::InvBlockSize);
				W.SpawnParticles(TEXT("water_drip"), P.Center() * MC::InvBlockSize + FVector(0, 0, 0.5), 20, 0.6f, FVector::ZeroVector, FColor(80, 120, 220));
			}
			return;
		}
		/** Dry a wet sponge in a furnace is handled by smelting recipes; in the nether it dries instantly. */
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (FMCBlocks::GetByState(S).Name == TEXT("wet_sponge") && W.Dim == EMCDimension::Nether)
				W.SetState(P, StateOf(TEXT("sponge")), MCSet_Render);
		}
	};

	// ================================================================================================================
	/** Copper oxidation with waxing (wax on handled by using honeycomb). */
	class FCopperOxidizeBeh : public FMCBlockBehavior
	{
	public:
		static bool IsWaxed(const FMCBlock& B) { return B.Name.ToString().StartsWith(TEXT("waxed_")); }
		static int32 StageOf(const FMCBlock& B)
		{
			const FString V = B.Variant.ToString();
			return V.IsNumeric() ? FCString::Atoi(*V) : 0;
		}
		static FName StageName(const FMCBlock& B, int32 Stage)
		{
			FString Base = B.Name.ToString();
			const bool bWaxed = Base.RemoveFromStart(TEXT("waxed_"));
			Base.RemoveFromStart(TEXT("oxidized_")); Base.RemoveFromStart(TEXT("weathered_")); Base.RemoveFromStart(TEXT("exposed_"));
			// the full block is "copper_block" when fresh but "exposed_copper" / "weathered_copper" / "oxidized_copper" later
			if (Base == TEXT("copper_block")) Base = TEXT("copper");
			const FString Waxed = bWaxed ? TEXT("waxed_") : TEXT("");
			if (Base == TEXT("copper") && Stage <= 0) return FName(*(Waxed + TEXT("copper_block")));
			static const TCHAR* Prefix[4] = { TEXT(""), TEXT("exposed_"), TEXT("weathered_"), TEXT("oxidized_") };
			return FName(*(Waxed + Prefix[FMath::Clamp(Stage, 0, 3)] + Base));
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (IsWaxed(B)) return;
			const int32 Stage = StageOf(B);
			if (Stage >= 3) return;
			int32 Mod = 0;
			// faster when adjacent to an already oxidised block or when not touching other copper
			for (int32 F = 0; F < 6; ++F)
			{
				const FMCBlock& N = FMCBlocks::GetByState(W.GetState(P.Offset((EMCFace)F)));
				if (N.Family == TEXT("copper") || N.Family == TEXT("cut_copper") || N.Family == TEXT("chiseled_copper"))
					Mod += N.Name.ToString().Contains(TEXT("oxidized")) ? -1 : (N.Name.ToString().Contains(TEXT("weathered")) ? 0 : 1);
			}
			const int32 Chance = (Mod + 1 + (Stage + 1)) * (Stage + 1) + (W.Rand.NextInt(64) == 0 ? 1 : 0);
			if (R.NextInt(64) >= Chance) return;
			const FMCState Next = StateOf(*StageName(B, Stage + 1).ToString());
			if (Next) W.SetState(P, Next, MCSet_Default | MCSet_KeepEntity);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!MCBeh::HeldIs(Player, TEXT("honeycomb"))) return false;
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (IsWaxed(B)) return false;
			W.SetState(P, StateOf(*(FString(TEXT("waxed_")) + B.Name.ToString())), MCSet_Default | MCSet_KeepEntity);
			if (!Player->IsCreative()) Player->ConsumeHeld(1);
			W.PlaySound(TEXT("copper_wax_on"), P.Center() * MC::InvBlockSize);
			W.SpawnParticles(TEXT("wax_on"), P.Center() * MC::InvBlockSize, 10, 0.4f, FVector::ZeroVector, FColor(240, 200, 80));
			return true;
		}
	};

	class FBuddingBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.NextInt(5) != 0) return;
			// grow a cluster on a random free face
			const EMCFace F = (EMCFace)R.NextInt(6);
			const FMCBlockPos Q = P.Offset(F);
			if (W.GetState(Q) != 0) return;
			const FMCState Cluster = MCBeh::StateOf(TEXT("amethyst_cluster"), (uint8)F);
			if (!FMCBlocks::GetByState(W.GetState(Q)).Behavior->IsFluidReplaceable(W.GetState(Q)))
				if (!MCBehaviorUtil::IsFaceSturdy(W, P, F)) return;
			W.SetState(Q, Cluster);
			W.PlaySound(TEXT("amethyst_resonate"), Q.Center() * MC::InvBlockSize, 0.6f, 0.7f);
		}
	};

	/** Pointed dripstone & sulfur spikes: grow downwards/upwards, drips, and stalactite falls. */
	class FDripstoneBeh : public FMCBlockBehavior
	{
	public:
		static int32 Thickness(FMCState S) { return (FMCBlocks::MetaOf(S) >> 1) & 3; }
		static bool IsDown(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0); }
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (Thickness(S) == 0) return true;
			const FMCBlockPos Support = IsDown(S) ? P.Up() : P.Down();
			const FMCState SS = W.GetState(Support);
			if (FMCBlocks::GetByState(SS).Name == B.Name) return true;
			return IsDown(S) ? MCBehaviorUtil::IsFaceSturdy(W, Support, EMCFace::Down) : MCBehaviorUtil::IsFaceSturdy(W, Support, EMCFace::Up);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const bool bDown = IsDown(S);
			const int32 Th = Thickness(S);
			const FMCBlockPos Tip = bDown ? P.Down() : P.Up();
			if (W.GetState(Tip) == 0)
			{
				// grow / thicken
				if (R.NextInt(3) == 0)
				{
					const int32 NewTh = Th < 3 ? Th + 1 : Th;
					W.SetState(P, WithMeta(S, (uint8)((bDown ? 1 : 0) | (NewTh << 1))), MCSet_Render);
					W.SetState(Tip, WithMeta(S, (uint8)((bDown ? 1 : 0) | ((Th + 1 > 3 ? 3 : Th + 1) << 1))));
				}
				return;
			}
			// drip particles / water from above a stalactite
			if (bDown && B.Name == TEXT("pointed_dripstone") && W.GetState(P.Up()) == FMCBlocks::C.Water && R.Chance(0.1))
				W.SpawnParticles(TEXT("drip_water"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.1), 1, 0.1f, FVector::ZeroVector, FColor(60, 90, 220));
			// falling stalactite when the support is broken is handled by CanSurvive
		}
		virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const override
		{
			if (!IsDown(S)) return;
			if (E && E->IsLiving() && Thickness(S) > 0) E->Hurt(FMCDamage::Of(TEXT("stalagmite")), 2.f * Thickness(S));
		}
	};

	/** Potent sulfur (26.2): glows, ignites nearby flammable blocks, pops when disturbed. */
	class FPotentSulfurBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.NextInt(12) != 0) return;
			// ignite a random flammable neighbour
			const FMCBlockPos Q = P.Offset((EMCFace)R.NextInt(6));
			const FMCState QS = W.GetState(Q);
			if (QS == 0)
			{
				if (R.Chance(0.8)) return;
				if (MCBeh::StateOf(TEXT("fire"))) W.SetState(Q, MCBeh::StateOf(TEXT("fire")));
				return;
			}
			const FMCBlock& QB = FMCBlocks::GetByState(QS);
			if (QB.FireBurn > 0 && R.NextInt(100) < QB.FireBurn) W.SetState(Q, MCBeh::StateOf(TEXT("fire")));
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || !E->IsLiving()) return;
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("potent_sulfur") && !E->bFireImmune) E->SetOnFire(3);
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.Chance(0.08)) W.SpawnParticles(TEXT("sulfur_spark"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + R.NextFloat()), 1, 0.f, FVector(0, 0, 0.02f), FColor(230, 220, 80));
		}
	};

	/** Creaking heart (26.x): a pale-oak heart that binds a Creaking mob at night. */
	class FCreakingHeartBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (!W.Game) return;
			const bool bDay = W.Game->IsDay();
			const uint8 M = FMCBlocks::MetaOf(S);
			// awake at night when connected to pale oak logs above/below
			const bool bLogs = FMCBlocks::GetByState(W.GetState(P.Up())).Name == TEXT("pale_oak_log") && FMCBlocks::GetByState(W.GetState(P.Down())).Name == TEXT("pale_oak_log");
			const bool bAwake = !bDay && bLogs;
			if (MCMeta::Bit(M, 0) != bAwake)
			{
				W.SetState(P, WithMeta(S, (uint8)((M & ~1) | (bAwake ? 1 : 0))), MCSet_Render);
				if (bAwake)
				{
					// spawn a creaking if none is nearby
					TArray<AMCEntity*> Near;
					W.GetEntitiesInBox(FMCBox(P.X - 16, P.Y - 16, P.Z - 16, P.X + 16, P.Y + 16, P.Z + 16), Near);
					bool bHas = false;
					for (AMCEntity* E : Near) if (E && E->TypeId == TEXT("creaking")) bHas = true;
					if (!bHas) W.SpawnMob(TEXT("creaking"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 1), true);
				}
				else
				{
					// banish creakings bound to this heart
					TArray<AMCEntity*> Near;
					W.GetEntitiesInBox(FMCBox(P.X - 32, P.Y - 32, P.Z - 32, P.X + 32, P.Y + 32, P.Z + 32), Near);
					for (AMCEntity* E : Near) if (E && E->TypeId == TEXT("creaking")) E->Discard();
				}
			}
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 0) && R.Chance(0.2))
				W.SpawnParticles(TEXT("sculk_soul"), P.Center() * MC::InvBlockSize, 1, 0.3f, FVector::ZeroVector, FColor(255, 160, 60));
		}
	};

	class FScaffoldingBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return Below != 0 && !FMCBlocks::IsFluid(Below);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.DestroyBlock(P, true);
		}
	};

	class FDragonEggBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnAttack(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Teleport(W, P, S); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override { Teleport(W, P, S); return true; }
		static void Teleport(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			W.SetState(P, 0);
			FMCRandom& R = W.Rand;
			for (int32 i = 0; i < 16; ++i)
			{
				const FMCBlockPos Q(P.X + R.Range(-7, 7), P.Y + R.Range(-7, 7), P.Z + R.Range(-7, 7));
				if (W.GetState(Q) != 0) continue;
				if (W.GetState(Q.Down()) == 0) continue;
				W.SetState(Q, S);
				W.PlaySound(TEXT("dragon_egg_teleport"), Q.Center() * MC::InvBlockSize);
				W.SpawnParticles(TEXT("portal"), Q.Center() * MC::InvBlockSize, 20, 0.5f, FVector::ZeroVector, FColor(150, 60, 220));
				return;
			}
			W.SetState(P, S);
			W.PlaySound(TEXT("dragon_egg_teleport"), P.Center() * MC::InvBlockSize);
		}
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override { Teleport(W, P, S); }
	};

	class FBedrockBeh : public FMCBlockBehavior
	{
	public:
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (W.Dim != EMCDimension::End && !(Tool && Tool->Item().Name == FName(TEXT("bedrock")))) return true;
			Out.Add(FMCItemStack::Of(TEXT("bedrock"), 1));
			return true;
		}
	};
}

void MCBeh::RegisterWorld()
{
	FFluidBeh* Water = new FFluidBeh(); Water->bLava = false;
	FFluidBeh* Lava = new FFluidBeh(); Lava->bLava = true;
	Register(EMCBeh::Water, Water);
	Register(EMCBeh::Lava, Lava);
	Register(EMCBeh::Falling, new FFallingBeh());
	Register(EMCBeh::ConcretePowder, new FConcretePowderBeh());
	Register(EMCBeh::Ice, new FIceBeh());
	Register(EMCBeh::SnowLayer, new FSnowLayerBeh());
	Register(EMCBeh::Fire, new FFireBeh());
	Register(EMCBeh::NetherPortal, new FNetherPortalBeh());
	Register(EMCBeh::EndPortal, new FEndPortalBeh());
	Register(EMCBeh::EndPortalFrame, new FEndPortalFrameBeh());
	Register(EMCBeh::EndGateway, new FEndGatewayBeh());
	Register(EMCBeh::Magma, new FMagmaBeh());
	Register(EMCBeh::SoulSand, new FSoulSandBeh());
	Register(EMCBeh::Honey, new FHoneyBeh());
	Register(EMCBeh::Slime, new FSlimeBeh());
	Register(EMCBeh::Cobweb, new FCobwebBeh());
	Register(EMCBeh::PowderSnow, new FPowderSnowBeh());
	Register(EMCBeh::Sponge, new FSpongeBeh());
	Register(EMCBeh::CopperOxidize, new FCopperOxidizeBeh());
	Register(EMCBeh::Budding, new FBuddingBeh());
	Register(EMCBeh::PointedDripstone, new FDripstoneBeh());
	Register(EMCBeh::PotentSulfur, new FPotentSulfurBeh());
	Register(EMCBeh::CreakingHeart, new FCreakingHeartBeh());
	Register(EMCBeh::Scaffolding, new FScaffoldingBeh());
	Register(EMCBeh::DragonEgg, new FDragonEggBeh());
	Register(EMCBeh::Bedrock, new FBedrockBeh());
}
