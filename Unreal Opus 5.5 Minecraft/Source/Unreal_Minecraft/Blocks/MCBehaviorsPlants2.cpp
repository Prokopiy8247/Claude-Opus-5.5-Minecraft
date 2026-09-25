// Plants & farming (part 2): grass/mycelium spreading, farmland, leaves decay, vines, mushrooms, nether plants,
// berries, cave vines, dripleaf, chorus, lily pads, glow lichen, aquatic plants, and bone meal.
#include "Blocks/MCBehaviorsInternal.h"
#include "Gen/MCFeatures.h"
#include "Gen/MCGenCommon.h"

using namespace MCBeh;

namespace MCBehPlants
{
	bool IsSoil(FMCState S);
	bool IsSandLike(FMCState S);
	bool IsNetherSoil(FMCState S);
	bool HasWaterNear(const FMCWorld& W, const FMCBlockPos& P, int32 R, int32 DZ0, int32 DZ1);
	int32 LightAt(const FMCWorld& W, const FMCBlockPos& P);
	int32 Binomial(FMCRandom& R, int32 N, float P);
	int32 ToolFortune(const FMCItemStack* Tool);
	bool ToolIs(const FMCItemStack* Tool, const TCHAR* Name);
	bool ToolSilk(const FMCItemStack* Tool);
	void RegisterPart1();
	bool GrowSapling(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R);
	bool GrowBamboo(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R);
}

using namespace MCBehPlants;

namespace
{
	const EMCFace GHoriz[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };

	// ================================================================================================================
	/** Grass block & mycelium: die in darkness, spread to nearby dirt, snowy flag from the block above. */
	class FSpreadingSoilBeh : public FMCBlockBehavior
	{
	public:
		bool bMycelium = false;
		static bool CanStayAlive(FMCWorld& W, const FMCBlockPos& P)
		{
			const FMCState Above = W.GetState(P.Up());
			if (FMCBlocks::GetByState(Above).Model == EMCModel::SnowLayer && (FMCBlocks::MetaOf(Above) & 7) == 0) return true;
			if (FMCBlocks::IsFluid(Above)) return false;
			return FMCBlocks::Info(Above).Opacity < 15 || Above == 0;
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (!CanStayAlive(W, P)) { W.SetState(P, FMCBlocks::C.Dirt); return; }
			if (LightAt(W, P.Up()) < 9) return;
			const FMCState Spread = bMycelium ? FMCBlocks::C.Mycelium : FMCBlocks::C.Grass;
			for (int32 i = 0; i < 4; ++i)
			{
				const FMCBlockPos Q(P.X + R.Range(-1, 1), P.Y + R.Range(-1, 1), P.Z + R.Range(-3, 1));
				if (W.GetState(Q) == FMCBlocks::C.Dirt && CanStayAlive(W, Q) && LightAt(W, Q.Up()) >= 4) W.SetState(Q, Spread);
			}
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (bMycelium || From != P.Up()) return;
			const FMCState Above = W.GetState(P.Up());
			const bool bSnow = FMCBlocks::GetByState(Above).Model == EMCModel::SnowLayer || Above == FMCBlocks::C.SnowBlock || Above == FMCBlocks::C.PowderSnow;
			const uint8 M = FMCBlocks::MetaOf(S);
			if (MCMeta::Bit(M, 0) != bSnow) W.SetState(P, WithMeta(S, MCMeta::SetBit(M, 0, bSnow)), MCSet_Render);
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (bMycelium && R.Chance(0.1)) W.SpawnParticles(TEXT("mycelium"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z + 1.1), 1, 0.f, FVector::ZeroVector, FColor(140, 110, 140));
		}
	};

	// ================================================================================================================
	class FFarmlandBeh : public FMCBlockBehavior
	{
	public:
		static void TurnToDirt(FMCWorld& W, const FMCBlockPos& P)
		{
			W.SetState(P, FMCBlocks::C.Dirt);
			// entities standing on it are pushed up by the height difference
			TArray<AMCEntity*> Ents;
			W.GetEntitiesInBox(FMCBox(P.X, P.Y, P.Z + 0.9, P.X + 1, P.Y + 1, P.Z + 1.1), Ents);
			for (AMCEntity* E : Ents) if (E) E->SetPosition(E->Pos + FVector(0, 0, 0.0625), false);
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (C.World && FMCBlocks::IsSolid(C.World->GetState(C.Pos.Up()))) return FMCBlocks::C.Dirt;
			return B.BaseState;
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const int32 Moist = FMCBlocks::MetaOf(S) & 7;
			const bool bWater = HasWaterNear(W, P, 4, 0, 1) || MCBeh::IsRainingAbove(W, P.Up());
			if (bWater) { if (Moist < 7) W.SetState(P, WithMeta(S, 7), MCSet_Render); return; }
			if (Moist > 0) { W.SetState(P, WithMeta(S, (uint8)(Moist - 1)), MCSet_Render); return; }
			const FMCBlock& Above = FMCBlocks::GetByState(W.GetState(P.Up()));
			const bool bCrop = Above.Has(MCB_Plant) && (Above.Behavior == MCBehaviors::Get(EMCBeh::Crop) || Above.Behavior == MCBehaviors::Get(EMCBeh::Stem));
			if (!bCrop) TurnToDirt(W, P);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (From == P.Up() && FMCBlocks::IsSolid(W.GetState(P.Up())) && !FMCBlocks::GetByState(W.GetState(P.Up())).Has(MCB_Plant)) TurnToDirt(W, P);
		}
		virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const override
		{
			if (!E || Distance < 0.5f) return;
			const bool bLiving = E->IsLiving();
			if (!bLiving) return;
			if (W.Rand.NextFloat() < Distance - 0.5f)
			{
				if (E->Kind == EMCEntityKind::Player || (W.Game && W.Game->Rules.bMobGriefing)) TurnToDirt(W, P);
			}
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("dirt"), 1));
			return true;
		}
	};

	// ================================================================================================================
	/** Leaves: distance to the nearest log (bits 0-2, 0 = unknown/natural), bit 3 = persistent. */
	class FLeavesBeh : public FMCBlockBehavior
	{
	public:
		static int32 ComputeDistance(FMCWorld& W, const FMCBlockPos& Start)
		{
			TArray<TPair<FMCBlockPos, int32>> Queue;
			TSet<FMCBlockPos> Seen;
			Queue.Add(TPair<FMCBlockPos, int32>(Start, 0));
			Seen.Add(Start);
			for (int32 i = 0; i < Queue.Num() && i < 400; ++i)
			{
				const FMCBlockPos P = Queue[i].Key;
				const int32 D = Queue[i].Value;
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F);
					if (Seen.Contains(Q)) continue;
					Seen.Add(Q);
					const FMCState S = W.GetState(Q);
					if (S == 0) continue;
					const uint32 Fl = FMCBlocks::Info(S).Flags;
					if (Fl & MCB_Log) return D + 1;
					if ((Fl & MCB_Leaves) && D + 1 < 7) Queue.Add(TPair<FMCBlockPos, int32>(Q, D + 1));
				}
			}
			return 7;
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override { return B.State(8 | 1); }
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			if (MCMeta::Bit(M, 3)) return;
			int32 D = M & 7;
			if (D == 0 || D == 7)
			{
				D = ComputeDistance(W, P);
				if (D < 7) { W.SetState(P, WithMeta(S, (uint8)D), MCSet_None); return; }
				// decay
				W.DestroyBlock(P, true, nullptr, nullptr, false);
			}
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			if (MCMeta::Bit(M, 3)) return;
			const FMCState FS = W.GetState(From);
			// a log or leaf nearby was removed: forget the cached distance so the next random tick re-evaluates
			if (FS == 0 || !(FMCBlocks::Info(FS).Flags & (MCB_Log | MCB_Leaves)))
				if ((M & 7) != 0) W.SetState(P, WithMeta(S, (uint8)(M & 8)), MCSet_None);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (ToolIs(Tool, TEXT("shears")) || ToolSilk(Tool)) { Out.Add(FMCItemStack::Of(B.Name, 1)); return true; }
			const FString Fam = B.Family.ToString();
			const int32 Fortune = ToolFortune(Tool);
			static const float SaplingChance[5] = { 0.05f, 0.0625f, 0.083333f, 0.1f, 0.1f };
			float SChance = SaplingChance[FMath::Min(Fortune, 4)];
			if (Fam == TEXT("jungle")) SChance *= 0.5f;
			FName Sapling = FName(*(Fam + TEXT("_sapling")));
			if (Fam == TEXT("mangrove")) Sapling = NAME_None;
			if (Fam == TEXT("azalea")) Sapling = B.Name == TEXT("flowering_azalea_leaves") ? FName(TEXT("flowering_azalea")) : FName(TEXT("azalea"));
			if (!Sapling.IsNone() && R.NextFloat() < SChance) Out.Add(FMCItemStack::Of(Sapling, 1));
			if (R.NextFloat() < 0.02f + Fortune * 0.0022f) Out.Add(FMCItemStack::Of(TEXT("stick"), 1 + R.NextInt(2)));
			if ((Fam == TEXT("oak") || Fam == TEXT("dark_oak")) && R.NextFloat() < 0.005f + Fortune * 0.0006f) Out.Add(FMCItemStack::Of(TEXT("apple"), 1));
			return true;
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (W.GetState(P.Down()) != 0) return;
			if (B.Family == TEXT("cherry") && R.Chance(0.1))
				W.SpawnParticles(TEXT("cherry_leaves"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z - 0.05), 1, 0.f, FVector(0, 0, -0.02f), FColor(255, 180, 200));
			else if (R.Chance(0.01))
				W.SpawnParticles(TEXT("falling_leaf"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z - 0.05), 1, 0.f, FVector(0, 0, -0.02f), FColor(90, 140, 40));
			if (MCBeh::IsRainingAbove(W, P.Up()) && R.Chance(0.07))
				W.SpawnParticles(TEXT("drip_water"), FVector(P.X + R.NextFloat(), P.Y + R.NextFloat(), P.Z - 0.05), 1, 0.f, FVector::ZeroVector, FColor(60, 90, 220));
		}
	};

	// ================================================================================================================
	/** Vines: bit per supporting side N,S,W,E (+ bit 4 up). */
	class FVineBeh : public FMCBlockBehavior
	{
	public:
		static bool CanAttach(const FMCWorld& W, const FMCBlockPos& P, EMCFace Dir)
		{
			const FMCBlockPos Q = P.Offset(Dir);
			return MCBehaviorUtil::IsFaceSturdy(W, Q, MC::Opposite(Dir)) || (FMCBlocks::Info(W.GetState(Q)).Flags & MCB_Leaves);
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			uint8 M = FMCBlocks::BlockOf(C.Existing) == B.Id ? FMCBlocks::MetaOf(C.Existing) : 0;
			const EMCFace Toward = MC::Opposite(C.ClickedFace); // direction from the vine to the clicked block
			if (MC::IsHorizontal(Toward) && CanAttach(*C.World, C.Pos, Toward)) M |= 1 << ((int32)Toward - 2);
			else if (Toward == EMCFace::Up && CanAttach(*C.World, C.Pos, EMCFace::Up)) M |= 16;
			else
				for (int32 i = 0; i < 4; ++i) if (CanAttach(*C.World, C.Pos, GHoriz[i])) { M |= 1 << i; break; }
			return M ? B.State(M) : 0;
		}
		static uint8 Supported(const FMCWorld& W, const FMCBlockPos& P, uint8 M)
		{
			uint8 Out = 0;
			const FMCState Above = W.GetState(P.Up());
			const bool bAboveVine = FMCBlocks::GetByState(Above).Model == EMCModel::Vine;
			for (int32 i = 0; i < 4; ++i)
			{
				if (!MCMeta::Bit(M, i)) continue;
				// hanging vine keeps a side if the vine above has the same side
				if (CanAttach(W, P, GHoriz[i]) || (bAboveVine && MCMeta::Bit(FMCBlocks::MetaOf(Above), i))) Out |= 1 << i;
			}
			if (MCMeta::Bit(M, 4) && CanAttach(W, P, EMCFace::Up)) Out |= 16;
			return Out;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			const uint8 NM = Supported(W, P, M);
			if (NM == M) return;
			if (NM == 0) W.DestroyBlock(P, false);
			else W.SetState(P, WithMeta(S, NM), MCSet_Render);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.NextInt(4) != 0) return;
			const uint8 M = FMCBlocks::MetaOf(S);
			// grow downwards
			const FMCBlockPos Below = P.Down();
			if (W.GetState(Below) == 0 && R.Chance(0.5))
			{
				const uint8 Down = (uint8)(M & 15 & R.NextInt(16));
				if (Down) W.SetState(Below, WithMeta(S, Down));
				return;
			}
			// grow sideways onto adjacent walls
			const int32 D = R.NextInt(4);
			const FMCBlockPos Side = P.Offset(GHoriz[D]);
			if (W.GetState(Side) == 0)
			{
				for (int32 i = 0; i < 4; ++i)
					if (CanAttach(W, Side, GHoriz[i])) { W.SetState(Side, WithMeta(S, (uint8)(1 << i))); return; }
			}
			else if (!MCMeta::Bit(M, D) && CanAttach(W, P, GHoriz[D])) W.SetState(P, WithMeta(S, (uint8)(M | (1 << D))), MCSet_Render);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(TEXT("vine"), 1));
			return true;
		}
	};

	// ================================================================================================================
	class FMushroomBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			const FName BN = FMCBlocks::GetByState(Below).Name;
			if (BN == TEXT("mycelium") || BN == TEXT("podzol") || BN == TEXT("crimson_nylium") || BN == TEXT("warped_nylium")) return true;
			return FMCBlocks::IsOpaque(Below) && W.GetLight(P, 0) < 13;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.NextInt(25) != 0) return;
			const FMCBlockId Id = FMCBlocks::BlockOf(S);
			int32 Count = 0;
			for (int32 dz = -1; dz <= 1; ++dz) for (int32 dy = -4; dy <= 4; ++dy) for (int32 dx = -4; dx <= 4; ++dx)
				if (FMCBlocks::BlockOf(W.GetState(FMCBlockPos(P.X + dx, P.Y + dy, P.Z + dz))) == Id && ++Count >= 5) return;
			const FMCBlockPos Q(P.X + R.Range(-1, 1), P.Y + R.Range(-1, 1), P.Z + R.Range(-1, 1));
			if (W.GetState(Q) == 0 && CanSurvive(FMCBlocks::GetByState(S), W, Q, S)) W.SetState(Q, S);
		}
	};

	class FNetherWartBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return W.GetState(P.Down()) == FMCBlocks::C.SoulSand; }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age < 3 && R.NextInt(10) == 0) W.SetState(P, WithMeta(S, (uint8)(Age + 1)), MCSet_Render);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const bool bMature = FMCBlocks::MetaOf(S) >= 3;
			Out.Add(FMCItemStack::Of(TEXT("nether_wart"), bMature ? 2 + R.NextInt(3 + ToolFortune(Tool)) : 1));
			return true;
		}
	};

	class FNetherPlantBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return IsNetherSoil(W.GetState(P.Down())); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("nether_sprouts")) { if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(B.Name, 1)); return true; }
			Out.Add(FMCItemStack::Of(B.Name, 1));
			return true;
		}
	};

	// ================================================================================================================
	class FSweetBerryBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return IsSoil(W.GetState(P.Down())); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age < 3 && R.NextInt(5) == 0 && LightAt(W, P.Up()) >= 9) W.SetState(P, WithMeta(S, (uint8)(Age + 1)), MCSet_Render);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age < 2) return false;
			if (Age == 3 || HeldIs(Player, TEXT("bone_meal")) == false)
			{
				MCBehaviorUtil::PopItem(W, P, FMCItemStack::Of(TEXT("sweet_berries"), 1 + W.Rand.NextInt(2) + (Age == 3 ? 1 : 0)));
				W.SetState(P, WithMeta(S, 1), MCSet_Render);
				W.PlaySound(TEXT("sweet_berry_pick"), P.Center() * MC::InvBlockSize);
				return true;
			}
			return false;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E || !E->IsLiving() || E->TypeId == TEXT("fox") || E->TypeId == TEXT("bee")) return;
			E->StuckSpeedMultiplier = FVector(0.8, 0.8, 0.75);
			if (FMCBlocks::MetaOf(S) > 0 && (FMath::Abs(E->Pos.X - E->PrevPos.X) >= 0.003 || FMath::Abs(E->Pos.Y - E->PrevPos.Y) >= 0.003))
				E->Hurt(FMCDamage::Of(TEXT("sweet_berry_bush")), 1.f);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age == 3) Out.Add(FMCItemStack::Of(TEXT("sweet_berries"), 2 + R.NextInt(2)));
			else if (Age == 2) Out.Add(FMCItemStack::Of(TEXT("sweet_berries"), 1 + R.NextInt(2)));
			return true;
		}
	};

	// ================================================================================================================
	class FCaveVinesBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Above = W.GetState(P.Up());
			return FMCBlocks::BlockOf(Above) == B.Id || MCBehaviorUtil::IsFaceSturdy(W, P.Up(), EMCFace::Down);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) return false;
			MCBehaviorUtil::PopItem(W, P, FMCItemStack::Of(TEXT("glow_berries"), 1));
			W.SetState(P, WithMeta(S, 0), MCSet_Render | MCSet_Light);
			W.PlaySound(TEXT("cave_vines_pick"), P.Center() * MC::InvBlockSize);
			return true;
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlockPos Below = P.Down();
			if (W.GetState(Below) == 0 && R.NextInt(10) == 0)
			{
				int32 Len = 1;
				while (FMCBlocks::BlockOf(W.GetState(P.Up(Len))) == FMCBlocks::BlockOf(S) && Len < 26) ++Len;
				if (Len < 25) W.SetState(Below, WithMeta(S, R.Chance(0.11) ? 1 : 0));
			}
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0) ? 14 : 0; }
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) Out.Add(FMCItemStack::Of(TEXT("glow_berries"), 1));
			return true;
		}
	};

	class FDripleafBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return FMCBlocks::BlockOf(Below) == B.Id || IsSoil(Below) || FMCBlocks::GetByState(Below).Name == TEXT("clay") || FMCBlocks::GetByState(Below).Name == TEXT("moss_block");
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			// tilt: after standing on the leaf for a moment it stops supporting the entity (scheduled)
			if (!W.HasScheduledTick(P, FMCBlocks::BlockOf(S)) && (FMCBlocks::MetaOf(S) & 3) == 0) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 10);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const uint8 Tilt = FMCBlocks::MetaOf(S) & 3;
			if (Tilt < 3)
			{
				W.SetState(P, WithMeta(S, (uint8)(Tilt + 1)), MCSet_Render);
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), Tilt == 0 ? 10 : (Tilt == 1 ? 10 : 100));
				if (Tilt == 0) W.PlaySound(TEXT("big_dripleaf_tilt_down"), P.Center() * MC::InvBlockSize);
			}
			else
			{
				W.SetState(P, WithMeta(S, 0), MCSet_Render);
				W.PlaySound(TEXT("big_dripleaf_tilt_up"), P.Center() * MC::InvBlockSize);
			}
		}
	};

	// ================================================================================================================
	class FChorusPlantBeh : public FMCBlockBehavior
	{
	public:
		static bool IsChorus(FMCState S) { const FName N = FMCBlocks::GetByState(S).Name; return N == TEXT("chorus_plant") || N == TEXT("chorus_flower"); }
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			const bool bAboveOrBelowAir = W.GetState(P.Up()) == 0 || Below == 0;
			for (EMCFace F : GHoriz)
			{
				const FMCState N = W.GetState(P.Offset(F));
				if (FMCBlocks::GetByState(N).Name == TEXT("chorus_plant"))
				{
					if (!bAboveOrBelowAir) return false;
					const FMCState NB = W.GetState(P.Offset(F).Down());
					if (FMCBlocks::GetByState(NB).Name == TEXT("chorus_plant") || NB == FMCBlocks::C.EndStone) return true;
				}
			}
			return FMCBlocks::GetByState(Below).Name == TEXT("chorus_plant") || Below == FMCBlocks::C.EndStone;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
	};

	class FChorusFlowerBeh : public FChorusPlantBeh
	{
	public:
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const int32 Age = FMCBlocks::MetaOf(S) & 7;
			if (Age >= 5 || W.GetState(P.Up()) != 0 || P.Z >= MC::MaxZ - 1) return;
			// count plant height below
			int32 Height = 0;
			bool bOnEndStone = false;
			for (int32 i = 1; i < 10; ++i)
			{
				const FMCState Bl = W.GetState(P.Down(i));
				if (FMCBlocks::GetByState(Bl).Name == TEXT("chorus_plant")) { ++Height; continue; }
				bOnEndStone = Bl == FMCBlocks::C.EndStone;
				break;
			}
			const bool bGrowUp = Height < 2 || (Height < 4 && R.NextInt(Height + 1) < 2) || bOnEndStone && Height < 5;
			if (bGrowUp && W.GetState(P.Up(2)) == 0)
			{
				W.SetState(P, StateOf(TEXT("chorus_plant")));
				W.SetState(P.Up(), WithMeta(S, (uint8)Age));
				return;
			}
			if (Age < 4)
			{
				const int32 Branches = R.NextInt(4) + (Height == 0 ? 1 : 0);
				bool bAny = false;
				for (int32 b = 0; b < Branches; ++b)
				{
					const EMCFace F = GHoriz[R.NextInt(4)];
					const FMCBlockPos Q = P.Offset(F);
					if (W.GetState(Q) == 0 && W.GetState(Q.Down()) == 0)
					{
						W.SetState(Q, WithMeta(S, (uint8)(Age + 1)));
						bAny = true;
					}
				}
				if (bAny) { W.SetState(P, StateOf(TEXT("chorus_plant"))); return; }
			}
			W.SetState(P, WithMeta(S, 5), MCSet_Render);
		}
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override
		{
			W.DestroyBlock(P, true, Projectile);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("chorus_flower"), 1));
			return true;
		}
	};

	// ================================================================================================================
	class FLilyPadBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return (Below == FMCBlocks::C.Water) || Below == FMCBlocks::C.Ice;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (E && E->Kind == EMCEntityKind::Boat) W.DestroyBlock(P, true, E);
		}
	};

	/** Glow lichen & sculk veins: one bit per attached face. */
	class FGlowLichenBeh : public FMCBlockBehavior
	{
	public:
		static bool CanAttach(const FMCWorld& W, const FMCBlockPos& P, EMCFace Dir)
		{
			return MCBehaviorUtil::IsFaceSturdy(W, P.Offset(Dir), MC::Opposite(Dir));
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			uint8 M = FMCBlocks::BlockOf(C.Existing) == B.Id ? FMCBlocks::MetaOf(C.Existing) : 0;
			const EMCFace Toward = MC::Opposite(C.ClickedFace);
			if (CanAttach(*C.World, C.Pos, Toward)) M |= 1 << (int32)Toward;
			return M ? B.State(M) : 0;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			uint8 NM = 0;
			for (int32 F = 0; F < 6; ++F) if (MCMeta::Bit(M, F) && CanAttach(W, P, (EMCFace)F)) NM |= 1 << F;
			if (NM == M) return;
			if (NM == 0) W.DestroyBlock(P, false);
			else W.SetState(P, WithMeta(S, NM), MCSet_Render);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			if (ToolIs(Tool, TEXT("shears")) || ToolSilk(Tool))
			{
				const int32 Faces = FMath::CountBits(FMCBlocks::MetaOf(S));
				Out.Add(FMCItemStack::Of(FMCBlocks::GetByState(S).Name, FMath::Max(1, Faces)));
			}
			return true;
		}
	};

	// ================================================================================================================
	/** Seagrass, kelp, corals, sea pickles: need water; breaking leaves water behind (handled by DestroyBlock for waterlogged blocks). */
	class FWaterPlantBeh : public FMCBlockBehavior
	{
	public:
		static bool InWater(const FMCWorld& W, const FMCBlockPos& P)
		{
			// the block itself is waterlogged; require water above or a water neighbour so it is not in open air
			const FMCState Above = W.GetState(P.Up());
			if (MCBehaviorUtil::IsWaterAt(W, P.Up())) return true;
			for (EMCFace F : GHoriz) if (MCBehaviorUtil::IsWaterAt(W, P.Offset(F))) return true;
			return Above != 0 && FMCBlocks::IsFluid(Above);
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.BaseState;
			const bool bWaterHere = C.Existing == FMCBlocks::C.Water || FMCBlocks::BlockOf(C.Existing) == FMCBlocks::C.WaterId;
			if (B.Name == TEXT("sea_pickle"))
			{
				if (FMCBlocks::BlockOf(C.Existing) == B.Id) return B.State((uint8)FMath::Min<int32>((FMCBlocks::MetaOf(C.Existing) & 3) + 1, 3));
				return MCBehaviorUtil::HasSolidTop(*C.World, C.Pos.Down()) ? B.State(0) : 0;
			}
			if (!bWaterHere) return (B.Name.ToString().EndsWith(TEXT("_coral"))) ? B.BaseState : 0;
			return B.BaseState;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			const FName N = B.Name;
			if (N == TEXT("kelp") || N == TEXT("kelp_plant"))
			{
				const FName BN = FMCBlocks::GetByState(Below).Name;
				return BN == TEXT("kelp_plant") || BN == TEXT("kelp") || MCBehaviorUtil::HasSolidTop(W, P.Down());
			}
			if (N == TEXT("tall_seagrass")) return MCBehaviorUtil::HasSolidTop(W, P.Down()) || FMCBlocks::GetByState(Below).Name == TEXT("tall_seagrass");
			return MCBehaviorUtil::HasSolidTop(W, P.Down());
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
			else if (FMCBlocks::GetByState(S).Name == TEXT("kelp") && FMCBlocks::GetByState(W.GetState(P.Up())).Name == TEXT("kelp"))
				W.SetState(P, StateOf(TEXT("kelp_plant")), MCSet_Render);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("kelp") && R.Chance(0.14))
			{
				int32 Len = 1;
				while (FMCBlocks::GetByState(W.GetState(P.Down(Len))).Name == TEXT("kelp_plant") && Len < 26) ++Len;
				const uint32 MaxLen = 5 + (MCHash::Hash2(0x6E1B, P.X, P.Y) % 20);
				if ((uint32)Len < MaxLen && W.GetState(P.Up()) == FMCBlocks::C.Water)
				{
					W.SetState(P, StateOf(TEXT("kelp_plant")), MCSet_Render);
					W.SetState(P.Up(), S);
				}
			}
			// corals die outside water
			if (B.Name.ToString().EndsWith(TEXT("_coral")) && !InWater(W, P) && R.Chance(0.2))
			{
				const FMCState Dead = StateOf(*(FString(TEXT("dead_")) + B.Name.ToString()));
				W.SetState(P, Dead ? Dead : FMCBlocks::C.Air);
			}
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("seagrass") || B.Name == TEXT("tall_seagrass")) { if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(TEXT("seagrass"), 1)); return true; }
			if (B.Name == TEXT("sea_pickle")) { Out.Add(FMCItemStack::Of(TEXT("sea_pickle"), (FMCBlocks::MetaOf(S) & 3) + 1)); return true; }
			if (B.Name.ToString().EndsWith(TEXT("_coral"))) { if (ToolSilk(Tool)) Out.Add(FMCItemStack::Of(B.Name, 1)); return true; }
			if (B.Name == TEXT("kelp") || B.Name == TEXT("kelp_plant")) { Out.Add(FMCItemStack::Of(TEXT("kelp"), 1)); return true; }
			return false;
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override
		{
			if (FMCBlocks::GetByState(S).Name == TEXT("sea_pickle")) return (uint8)FMath::Min(15, 6 + (FMCBlocks::MetaOf(S) & 3) * 3);
			return Default;
		}
	};
}

// ---------------------------------------------------------------------------------------------------------------------
// Bone meal

bool MCBeh::ApplyBoneMeal(FMCWorld& W, const FMCBlockPos& P, AMCPlayer* Player)
{
	const FMCState S = W.GetState(P);
	if (S == 0) return false;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	FMCRandom& R = W.Rand;
	auto Effect = [&]() { W.SpawnParticles(TEXT("happy_villager"), P.Center() * MC::InvBlockSize, 15, 0.5f, FVector::ZeroVector, FColor(120, 255, 120)); W.PlaySound(TEXT("bone_meal_use"), P.Center() * MC::InvBlockSize); };
	const FMCBlockBehavior* Beh = B.Behavior;

	if (Beh == MCBehaviors::Get(EMCBeh::Crop))
	{
		const int32 Max = B.NumStates - 1;
		const int32 Age = FMCBlocks::MetaOf(S);
		if (Age >= Max) return false;
		W.SetState(P, B.State((uint8)FMath::Min(Max, Age + R.Range(2, 5))), MCSet_Render);
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::Stem))
	{
		const int32 Age = FMCBlocks::MetaOf(S);
		if (Age >= 7) return false;
		W.SetState(P, B.State((uint8)FMath::Min(7, Age + R.Range(2, 5))), MCSet_Render);
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::Sapling))
	{
		if (R.Chance(0.45)) GrowSapling(W, P, S, R);
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::SweetBerryBush))
	{
		const int32 Age = FMCBlocks::MetaOf(S);
		if (Age >= B.NumStates - 1) return false;
		W.SetState(P, B.State((uint8)(Age + 1)), MCSet_Render);
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::Bamboo))
	{
		if (!GrowBamboo(W, P, S, R)) return false;
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::CaveVines))
	{
		if (MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) return false;
		W.SetState(P, B.State(1), MCSet_Render | MCSet_Light);
		Effect(); return true;
	}
	if (B.Name == TEXT("short_grass") || B.Name == TEXT("fern"))
	{
		if (W.GetState(P.Up()) != 0) return false;
		const FMCState Tall = StateOf(B.Name == TEXT("short_grass") ? TEXT("tall_grass") : TEXT("large_fern"));
		W.SetState(P, Tall, MCSet_Default | MCSet_NoSupport);
		W.SetState(P.Up(), WithMeta(Tall, 1), MCSet_Default | MCSet_NoSupport);
		Effect(); return true;
	}
	if (B.Behavior == MCBehaviors::Get(EMCBeh::TallPlant) && B.HasTag(TEXT("tall_flowers")))
	{
		MCBehaviorUtil::PopItem(W, P, FMCItemStack::Of(B.Name, 1));
		Effect(); return true;
	}
	if (S == FMCBlocks::C.Grass || B.Name == TEXT("moss_block") || B.Name == TEXT("pale_moss_block"))
	{
		// scatter grass & flowers (or spread moss)
		const bool bMoss = S != FMCBlocks::C.Grass;
		const uint8 Biome = W.GetBiome(P);
		for (int32 i = 0; i < 64; ++i)
		{
			FMCBlockPos Q = P;
			for (int32 j = 0; j < i / 16; ++j) Q = FMCBlockPos(Q.X + R.Range(-1, 1), Q.Y + R.Range(-1, 1), Q.Z + R.Range(-1, 1) * R.Range(0, 1));
			if (bMoss)
			{
				const FMCBlockPos QQ(P.X + R.Range(-3, 3), P.Y + R.Range(-3, 3), P.Z + R.Range(-1, 1));
				const FMCState QS = W.GetState(QQ);
				if ((FMCBlocks::Info(QS).Flags & (MCB_Stone | MCB_Dirt)) && W.GetState(QQ.Up()) == 0) W.SetState(QQ, S);
				continue;
			}
			if (W.GetState(Q) != FMCBlocks::C.Grass || W.GetState(Q.Up()) != 0) continue;
			const float Roll = R.NextFloat();
			FMCState Plant = StateOf(TEXT("short_grass"));
			if (Roll < 0.125f)
			{
				const TCHAR* Flowers[] = { TEXT("dandelion"), TEXT("poppy"), TEXT("azure_bluet"), TEXT("oxeye_daisy"), TEXT("cornflower") };
				Plant = StateOf(Flowers[R.NextInt(UE_ARRAY_COUNT(Flowers))]);
				if (Biome == (uint8)EMCBiome::FlowerForest || Biome == (uint8)EMCBiome::Meadow) Plant = StateOf(R.NextBool() ? TEXT("allium") : TEXT("lily_of_the_valley"));
				if (Biome == (uint8)EMCBiome::Swamp) Plant = StateOf(TEXT("blue_orchid"));
			}
			W.SetState(Q.Up(), Plant);
		}
		Effect(); return true;
	}
	if (B.Name == TEXT("brown_mushroom") || B.Name == TEXT("red_mushroom"))
	{
		if (!R.Chance(0.4)) { Effect(); return true; }
		W.SetState(P, 0);
		FMCRandom TR(R.Next());
		RunFeatureInWorld(W, P, 8, [&](FMCGenWriter& Wr) { FMCRandom C = TR; MCFeatures::Tree(Wr, C, B.Name == TEXT("red_mushroom") ? EMCTree::RedMushroom : EMCTree::BrownMushroom, P, true); });
		Effect(); return true;
	}
	if (B.Name == TEXT("crimson_fungus") || B.Name == TEXT("warped_fungus"))
	{
		const FName Soil = FMCBlocks::GetByState(W.GetState(P.Down())).Name;
		const bool bCrimson = B.Name == TEXT("crimson_fungus");
		if (Soil != (bCrimson ? FName(TEXT("crimson_nylium")) : FName(TEXT("warped_nylium")))) return false;
		if (R.Chance(0.4))
		{
			W.SetState(P, 0);
			FMCRandom TR(R.Next());
			RunFeatureInWorld(W, P, 8, [&](FMCGenWriter& Wr) { FMCRandom C = TR; MCFeatures::Tree(Wr, C, bCrimson ? EMCTree::CrimsonFungus : EMCTree::WarpedFungus, P, true); });
		}
		Effect(); return true;
	}
	if (B.Name == TEXT("crimson_nylium") || B.Name == TEXT("warped_nylium") || B.Name == TEXT("netherrack"))
	{
		const bool bCrimson = B.Name == TEXT("crimson_nylium");
		if (B.Name == TEXT("netherrack"))
		{
			// spreads nylium from a neighbour
			for (EMCFace F : GHoriz)
			{
				const FName N = FMCBlocks::GetByState(W.GetState(P.Offset(F))).Name;
				if (N == TEXT("crimson_nylium") || N == TEXT("warped_nylium")) { W.SetState(P, StateOf(*N.ToString())); Effect(); return true; }
			}
			return false;
		}
		for (int32 i = 0; i < 20; ++i)
		{
			const FMCBlockPos Q(P.X + R.Range(-3, 3), P.Y + R.Range(-3, 3), P.Z);
			if (W.GetState(Q) == S && W.GetState(Q.Up()) == 0)
				W.SetState(Q.Up(), StateOf(bCrimson ? (R.Chance(0.1) ? TEXT("crimson_fungus") : TEXT("crimson_roots")) : (R.Chance(0.1) ? TEXT("warped_fungus") : (R.NextBool() ? TEXT("warped_roots") : TEXT("nether_sprouts")))));
		}
		Effect(); return true;
	}
	if (B.Name == TEXT("sea_pickle") && MCBehaviorUtil::IsWaterAt(W, P))
	{
		for (int32 i = 0; i < 8; ++i)
		{
			const FMCBlockPos Q(P.X + R.Range(-2, 2), P.Y + R.Range(-2, 2), P.Z);
			if (W.GetState(Q) == FMCBlocks::C.Water && MCBehaviorUtil::HasSolidTop(W, Q.Down())) W.SetState(Q, StateOf(TEXT("sea_pickle"), (uint8)R.NextInt(4)));
		}
		Effect(); return true;
	}
	if (S == FMCBlocks::C.Water || B.Name == TEXT("seagrass"))
	{
		// underwater: grow seagrass around
		for (int32 i = 0; i < 16; ++i)
		{
			const FMCBlockPos Q(P.X + R.Range(-3, 3), P.Y + R.Range(-3, 3), P.Z + R.Range(-1, 1));
			if (W.GetState(Q) == FMCBlocks::C.Water && MCBehaviorUtil::HasSolidTop(W, Q.Down())) W.SetState(Q, StateOf(TEXT("seagrass")));
		}
		Effect(); return true;
	}
	if (Beh == MCBehaviors::Get(EMCBeh::GlowLichen))
	{
		for (EMCFace F : GHoriz)
		{
			const FMCBlockPos Q = P.Offset(F);
			if (W.GetState(Q) != 0) continue;
			for (int32 D = 0; D < 6; ++D)
				if (MCBehaviorUtil::IsFaceSturdy(W, Q.Offset((EMCFace)D), MC::Opposite((EMCFace)D))) { W.SetState(Q, B.State((uint8)(1 << D))); Effect(); return true; }
		}
		return false;
	}
	if (B.Name == TEXT("small_dripleaf") || B.Name == TEXT("big_dripleaf"))
	{
		if (W.GetState(P.Up()) != 0) return false;
		W.SetState(P.Up(), StateOf(TEXT("big_dripleaf")));
		if (B.Name == TEXT("small_dripleaf")) W.SetState(P, StateOf(TEXT("big_dripleaf")));
		Effect(); return true;
	}
	if (B.Name == TEXT("kelp") || B.Name == TEXT("kelp_plant"))
	{
		FMCBlockPos Top = P;
		while (FMCBlocks::GetByState(W.GetState(Top.Up())).Name == TEXT("kelp_plant") || FMCBlocks::GetByState(W.GetState(Top.Up())).Name == TEXT("kelp")) Top = Top.Up();
		if (W.GetState(Top.Up()) != FMCBlocks::C.Water) return false;
		W.SetState(Top, StateOf(TEXT("kelp_plant")));
		W.SetState(Top.Up(), StateOf(TEXT("kelp")));
		Effect(); return true;
	}
	if (B.Name == TEXT("sugar_cane") || B.Name == TEXT("cactus")) return false;
	return false;
}

void MCBeh::RegisterPlants()
{
	RegisterPart1();
	FSpreadingSoilBeh* Grass = new FSpreadingSoilBeh(); Grass->bMycelium = false;
	FSpreadingSoilBeh* Myc = new FSpreadingSoilBeh(); Myc->bMycelium = true;
	Register(EMCBeh::Grass, Grass);
	Register(EMCBeh::Mycelium, Myc);
	Register(EMCBeh::Farmland, new FFarmlandBeh());
	Register(EMCBeh::Leaves, new FLeavesBeh());
	Register(EMCBeh::Vine, new FVineBeh());
	Register(EMCBeh::Mushroom, new FMushroomBeh());
	Register(EMCBeh::NetherWart, new FNetherWartBeh());
	Register(EMCBeh::NetherPlant, new FNetherPlantBeh());
	Register(EMCBeh::SweetBerryBush, new FSweetBerryBeh());
	Register(EMCBeh::CaveVines, new FCaveVinesBeh());
	Register(EMCBeh::Dripleaf, new FDripleafBeh());
	Register(EMCBeh::ChorusPlant, new FChorusPlantBeh());
	Register(EMCBeh::ChorusFlower, new FChorusFlowerBeh());
	Register(EMCBeh::LilyPad, new FLilyPadBeh());
	Register(EMCBeh::GlowLichen, new FGlowLichenBeh());
	Register(EMCBeh::WaterPlant, new FWaterPlantBeh());
}
