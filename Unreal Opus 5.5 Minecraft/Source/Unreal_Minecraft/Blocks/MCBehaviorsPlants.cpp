// Plants & farming (part 1): generic plants, tall plants, crops, stems, saplings, sugar cane, cactus, bamboo.
#include "Blocks/MCBehaviorsInternal.h"
#include "Gen/MCFeatures.h"
#include "Gen/MCGenCommon.h"

using namespace MCBeh;

namespace MCBehPlants
{
	bool IsSoil(FMCState S)
	{
		if (S == 0) return false;
		const FMCBlock& B = FMCBlocks::GetByState(S);
		return B.Has(MCB_Soil) || B.Has(MCB_Dirt) || B.Model == EMCModel::Farmland;
	}

	bool IsSandLike(FMCState S)
	{
		if (S == 0) return false;
		const FMCBlock& B = FMCBlocks::GetByState(S);
		return B.Has(MCB_Sand) || B.Name == TEXT("suspicious_sand") || B.Name == TEXT("terracotta") || B.Variant == TEXT("terracotta");
	}

	bool IsNetherSoil(FMCState S)
	{
		if (S == 0) return false;
		const FName N = FMCBlocks::GetByState(S).Name;
		return N == TEXT("crimson_nylium") || N == TEXT("warped_nylium") || N == TEXT("soul_soil") || N == TEXT("soul_sand") || N == TEXT("netherrack")
			|| IsSoil(S);
	}

	bool HasWaterNear(const FMCWorld& W, const FMCBlockPos& P, int32 R, int32 DZ0, int32 DZ1)
	{
		for (int32 dz = DZ0; dz <= DZ1; ++dz)
			for (int32 dy = -R; dy <= R; ++dy)
				for (int32 dx = -R; dx <= R; ++dx)
					if (MCBehaviorUtil::IsWaterAt(W, FMCBlockPos(P.X + dx, P.Y + dy, P.Z + dz))) return true;
		return false;
	}

	int32 LightAt(const FMCWorld& W, const FMCBlockPos& P)
	{
		const int32 Darken = W.Game ? W.Game->GetSkyDarken() : 0;
		return W.GetLight(P, Darken);
	}

	/** Minecraft crop growth speed factor from the farmland around the crop. */
	float GrowthSpeed(const FMCWorld& W, const FMCBlockPos& P, FMCBlockId CropId)
	{
		float Speed = 1.f;
		const FMCBlockPos Below = P.Down();
		for (int32 dy = -1; dy <= 1; ++dy)
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				const FMCState S = W.GetState(FMCBlockPos(Below.X + dx, Below.Y + dy, Below.Z));
				float F = 0.f;
				if (FMCBlocks::GetByState(S).Model == EMCModel::Farmland) F = (FMCBlocks::MetaOf(S) & 7) > 0 ? 3.f : 1.f;
				if (dx != 0 || dy != 0) F /= 4.f;
				Speed += F;
			}
		// rows of the same crop slow each other down unless planted in rows
		const bool bNS = FMCBlocks::BlockOf(W.GetState(P.Offset(EMCFace::North))) == CropId || FMCBlocks::BlockOf(W.GetState(P.Offset(EMCFace::South))) == CropId;
		const bool bWE = FMCBlocks::BlockOf(W.GetState(P.Offset(EMCFace::West))) == CropId || FMCBlocks::BlockOf(W.GetState(P.Offset(EMCFace::East))) == CropId;
		if (bNS && bWE) Speed /= 2.f;
		return Speed;
	}

	int32 Binomial(FMCRandom& R, int32 N, float P)
	{
		int32 C = 0;
		for (int32 i = 0; i < N; ++i) if (R.NextFloat() < P) ++C;
		return C;
	}

	int32 ToolFortune(const FMCItemStack* Tool) { return Tool ? Tool->GetEnchant(EMCEnchant::Fortune) : 0; }
	bool ToolIs(const FMCItemStack* Tool, const TCHAR* Name) { return Tool && !Tool->IsEmpty() && Tool->Item().Name == FName(Name); }
	bool ToolSilk(const FMCItemStack* Tool) { return Tool && Tool->GetEnchant(EMCEnchant::SilkTouch) > 0; }

	/** Plant support: soil for normal plants, special rules by name. */
	bool CanPlantSurvive(const FMCBlock& B, const FMCWorld& W, const FMCBlockPos& P)
	{
		const FName N = B.Name;
		const FMCState Below = W.GetState(P.Down());
		const FMCState Above = W.GetState(P.Up());
		// hanging plants
		if (N == TEXT("hanging_roots") || N == TEXT("spore_blossom") || N == TEXT("weeping_vines") || N == TEXT("pale_hanging_moss"))
		{
			if (FMCBlocks::BlockOf(Above) == B.Id) return true;
			if (N == TEXT("pale_hanging_moss")) return Above != 0 && (FMCBlocks::Info(Above).Flags & (MCB_Leaves | MCB_Log | MCB_Opaque)) != 0;
			return MCBehaviorUtil::IsFaceSturdy(W, P.Up(), EMCFace::Down);
		}
		if (N == TEXT("twisting_vines"))
		{
			return FMCBlocks::BlockOf(Below) == B.Id || MCBehaviorUtil::IsFaceSturdy(W, P.Down(), EMCFace::Up);
		}
		if (N == TEXT("dead_bush")) return IsSandLike(Below) || IsSoil(Below);
		if (N == TEXT("wither_rose")) return IsSoil(Below) || IsNetherSoil(Below);
		if (N == TEXT("leaf_litter") || N == TEXT("wildflowers") || N == TEXT("pink_petals")) return MCBehaviorUtil::HasSolidTop(W, P.Down());
		if (N == TEXT("firefly_bush") || N == TEXT("bush")) return IsSoil(Below);
		if (N == TEXT("torchflower") || N == TEXT("open_eyeblossom") || N == TEXT("closed_eyeblossom")) return IsSoil(Below);
		return IsSoil(Below);
	}
}

using namespace MCBehPlants;

namespace
{
	// ================================================================================================================
	class FPlantBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return CanPlantSurvive(B, W, P); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const FName N = B.Name;
			if (N == TEXT("short_grass") || N == TEXT("fern"))
			{
				if (ToolIs(Tool, TEXT("shears"))) { Out.Add(FMCItemStack::Of(N, 1)); return true; }
				if (R.NextFloat() < 0.125f) Out.Add(FMCItemStack::Of(TEXT("wheat_seeds"), 1 + (ToolFortune(Tool) > 0 ? R.NextInt(ToolFortune(Tool) * 2 + 1) : 0)));
				return true;
			}
			if (N == TEXT("hanging_roots") || N == TEXT("pale_hanging_moss") || N == TEXT("leaf_litter"))
			{
				if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(N, 1));
				return true;
			}
			if (N == TEXT("weeping_vines") || N == TEXT("twisting_vines"))
			{
				if (ToolIs(Tool, TEXT("shears")) || ToolSilk(Tool) || R.NextFloat() < 0.33f) Out.Add(FMCItemStack::Of(N, 1));
				return true;
			}
			return false;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("wither_rose") && E && E->IsLiving() && W.Dim != EMCDimension::End)
			{
				AMCLiving* L = static_cast<AMCLiving*>(E);
				if (!L->HasEffect(EMCEffect::Wither)) { FMCEffectInstance I; I.Effect = EMCEffect::Wither; I.Duration = 40; L->AddEffect(I); }
			}
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			// eyeblossoms open at night and close by day
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (!W.Game) return;
			if (B.Name == TEXT("closed_eyeblossom") && !W.Game->IsDay()) W.SetState(P, StateOf(TEXT("open_eyeblossom")));
			else if (B.Name == TEXT("open_eyeblossom") && W.Game->IsDay()) W.SetState(P, StateOf(TEXT("closed_eyeblossom")));
		}
		virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("firefly_bush") && W.Game && !W.Game->IsDay() && R.Chance(0.3))
				W.SpawnParticles(TEXT("firefly"), FVector(P.X + R.FRange(-1.f, 2.f), P.Y + R.FRange(-1.f, 2.f), P.Z + R.FRange(0.2f, 2.f)), 1, 0.f, FVector::ZeroVector, FColor(220, 255, 120));
			if (B.Name == TEXT("spore_blossom") && R.Chance(0.5))
				W.SpawnParticles(TEXT("spore"), FVector(P.X + R.FRange(-6.f, 7.f), P.Y + R.FRange(-6.f, 7.f), P.Z - R.FRange(0.f, 8.f)), 1, 0.f, FVector(0, 0, -0.01f), FColor(180, 230, 120));
		}
	};

	// ================================================================================================================
	/** Two-block plants (tall grass, large fern, sunflower, lilac, rose bush, peony, pitcher plant, tall seagrass). */
	class FTallPlantBeh : public FMCBlockBehavior
	{
	public:
		static bool IsUpper(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.BaseState;
			const FMCState Above = C.World->GetState(C.Pos.Up());
			if (Above != 0 && !FMCBlocks::IsReplaceable(Above)) return 0;
			return B.State(0);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			W.SetState(P.Up(), WithMeta(S, 1), MCSet_Default | MCSet_NoSupport);
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (IsUpper(S)) return FMCBlocks::BlockOf(W.GetState(P.Down())) == B.Id && !IsUpper(W.GetState(P.Down()));
			if (B.Has(MCB_Waterlogged)) return MCBehaviorUtil::HasSolidTop(W, P.Down());
			return CanPlantSurvive(B, W, P) && FMCBlocks::BlockOf(W.GetState(P.Up())) == B.Id;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (CanSurvive(FMCBlocks::GetByState(S), W, P, S)) return;
			if (IsUpper(S)) W.SetState(P, FMCBlocks::GetByState(S).Has(MCB_Waterlogged) ? FMCBlocks::C.Water : 0);
			else BreakUnsupported(W, P);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (IsUpper(S)) return true;
			if (B.Name == TEXT("tall_grass") || B.Name == TEXT("large_fern"))
			{
				if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(B.Name == TEXT("tall_grass") ? TEXT("short_grass") : TEXT("fern"), 2));
				else if (R.NextFloat() < 0.125f) Out.Add(FMCItemStack::Of(TEXT("wheat_seeds"), 1));
				return true;
			}
			if (B.Name == TEXT("tall_seagrass")) { if (ToolIs(Tool, TEXT("shears"))) Out.Add(FMCItemStack::Of(TEXT("seagrass"), 2)); return true; }
			Out.Add(FMCItemStack::Of(B.Name, 1));
			return true;
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (FMCBlocks::BlockOf(New) == FMCBlocks::BlockOf(Old)) return;
			const FMCBlockPos Other = IsUpper(Old) ? P.Down() : P.Up();
			const FMCState OS = W.GetState(Other);
			if (FMCBlocks::BlockOf(OS) == FMCBlocks::BlockOf(Old))
				W.SetState(Other, FMCBlocks::GetByState(Old).Has(MCB_Waterlogged) ? FMCBlocks::C.Water : 0, MCSet_Default | MCSet_NoSupport);
		}
	};

	// ================================================================================================================
	/** Farmland crops: wheat, carrots, potatoes, beetroots, torchflower crop. Meta = age. */
	class FCropBeh : public FMCBlockBehavior
	{
	public:
		static int32 MaxAge(const FMCBlock& B) { return B.NumStates - 1; }
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return FMCBlocks::GetByState(W.GetState(P.Down())).Model == EMCModel::Farmland;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (LightAt(W, P) < 9) return;
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age >= MaxAge(B))
			{
				if (B.Name == TEXT("torchflower_crop")) W.SetState(P, StateOf(TEXT("torchflower")));
				return;
			}
			const float Speed = GrowthSpeed(W, P, B.Id);
			const int32 Chance = (int32)(25.f / Speed) + 1;
			if (R.NextInt(Chance) == 0) W.SetState(P, B.State((uint8)(Age + 1)), MCSet_Render);
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			// ravagers trample crops
			if (E && E->TypeId == TEXT("ravager") && W.Game && W.Game->Rules.bMobGriefing) W.DestroyBlock(P, true, E);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const bool bMature = (int32)FMCBlocks::MetaOf(S) >= MaxAge(B);
			const int32 Fortune = ToolFortune(Tool);
			const FName N = B.Name;
			if (N == TEXT("wheat"))
			{
				if (bMature) { Out.Add(FMCItemStack::Of(TEXT("wheat"), 1)); Out.Add(FMCItemStack::Of(TEXT("wheat_seeds"), 1 + Binomial(R, 3 + Fortune, 0.5714f))); }
				else Out.Add(FMCItemStack::Of(TEXT("wheat_seeds"), 1));
			}
			else if (N == TEXT("carrots"))
			{
				Out.Add(FMCItemStack::Of(TEXT("carrot"), bMature ? 2 + Binomial(R, 3 + Fortune, 0.5714f) : 1));
			}
			else if (N == TEXT("potatoes"))
			{
				Out.Add(FMCItemStack::Of(TEXT("potato"), bMature ? 2 + Binomial(R, 3 + Fortune, 0.5714f) : 1));
				if (bMature && R.NextFloat() < 0.02f) Out.Add(FMCItemStack::Of(TEXT("poisonous_potato"), 1));
			}
			else if (N == TEXT("beetroots"))
			{
				if (bMature) { Out.Add(FMCItemStack::Of(TEXT("beetroot"), 1)); Out.Add(FMCItemStack::Of(TEXT("beetroot_seeds"), 1 + Binomial(R, 3 + Fortune, 0.5714f))); }
				else Out.Add(FMCItemStack::Of(TEXT("beetroot_seeds"), 1));
			}
			else if (N == TEXT("torchflower_crop")) Out.Add(FMCItemStack::Of(TEXT("torchflower_seeds"), 1));
			return true;
		}
	};

	// ================================================================================================================
	/** Melon & pumpkin stems: grow to age 7 then place the fruit on an adjacent soil block. */
	class FStemBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return FMCBlocks::GetByState(W.GetState(P.Down())).Model == EMCModel::Farmland;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (LightAt(W, P) < 9) return;
			const float Speed = GrowthSpeed(W, P, B.Id);
			if (R.NextInt((int32)(25.f / Speed) + 1) != 0) return;
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age < 7) { W.SetState(P, B.State((uint8)(Age + 1)), MCSet_Render); return; }
			const bool bMelon = B.Name == TEXT("melon_stem");
			const FMCState Fruit = StateOf(bMelon ? TEXT("melon") : TEXT("pumpkin"));
			for (const EMCFace F : { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East })
				if (FMCBlocks::BlockOf(W.GetState(P.Offset(F))) == FMCBlocks::BlockOf(Fruit)) return;
			const EMCFace F = MC::HorizontalFace(R.NextInt(4));
			const FMCBlockPos T = P.Offset(F);
			if (W.GetState(T) == 0 && IsSoil(W.GetState(T.Down()))) W.SetState(T, Fruit);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const int32 Age = FMCBlocks::MetaOf(S);
			const int32 N = Binomial(R, 3, (Age + 1) / 15.f);
			if (N > 0) Out.Add(FMCItemStack::Of(B.Name == TEXT("melon_stem") ? TEXT("melon_seeds") : TEXT("pumpkin_seeds"), N));
			return true;
		}
	};

	// ================================================================================================================
	class FSaplingBeh : public FMCBlockBehavior
	{
	public:
		static bool IsSapling(FMCState S, FMCBlockId Id) { return FMCBlocks::BlockOf(S) == Id; }
		/** Find a 2x2 arrangement of the same sapling containing P. Returns the min corner. */
		static bool Find2x2(FMCWorld& W, const FMCBlockPos& P, FMCBlockId Id, FMCBlockPos& OutCorner)
		{
			for (int32 ox = -1; ox <= 0; ++ox)
				for (int32 oy = -1; oy <= 0; ++oy)
				{
					const FMCBlockPos C(P.X + ox, P.Y + oy, P.Z);
					if (IsSapling(W.GetState(C), Id) && IsSapling(W.GetState(C.Offset(EMCFace::East)), Id)
						&& IsSapling(W.GetState(C.Offset(EMCFace::South)), Id) && IsSapling(W.GetState(C.Offset(EMCFace::South).Offset(EMCFace::East)), Id))
					{
						OutCorner = C;
						return true;
					}
				}
			return false;
		}
		static bool HasRoom(FMCWorld& W, const FMCBlockPos& P, int32 Height, int32 Size)
		{
			for (int32 k = 1; k < Height; ++k)
				for (int32 a = 0; a < Size; ++a)
					for (int32 b = 0; b < Size; ++b)
					{
						const FMCState S = W.GetState(FMCBlockPos(P.X + a, P.Y + b, P.Z + k));
						if (S != 0 && !FMCBlocks::IsReplaceable(S) && !(FMCBlocks::Info(S).Flags & (MCB_Leaves | MCB_Plant))) return false;
					}
			return P.Z + Height < MC::MaxZ;
		}
		static bool Grow(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R)
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const FString Fam = B.Family.ToString();
			const FName N = B.Name;
			EMCTree Type = EMCTree::Oak;
			bool b2x2 = false, bRequire2x2 = false;
			FMCBlockPos Base = P;
			if (N == TEXT("mangrove_propagule")) Type = EMCTree::Mangrove;
			else if (N == TEXT("azalea") || N == TEXT("flowering_azalea")) Type = EMCTree::Azalea;
			else if (Fam == TEXT("oak")) Type = R.Chance(0.1) ? EMCTree::FancyOak : EMCTree::Oak;
			else if (Fam == TEXT("birch")) Type = EMCTree::Birch;
			else if (Fam == TEXT("spruce")) { Type = EMCTree::Spruce; b2x2 = true; }
			else if (Fam == TEXT("jungle")) { Type = EMCTree::Jungle; b2x2 = true; }
			else if (Fam == TEXT("acacia")) Type = EMCTree::Acacia;
			else if (Fam == TEXT("dark_oak")) { Type = EMCTree::DarkOak; b2x2 = true; bRequire2x2 = true; }
			else if (Fam == TEXT("pale_oak")) { Type = EMCTree::PaleOak; b2x2 = true; bRequire2x2 = true; }
			else if (Fam == TEXT("cherry")) Type = EMCTree::Cherry;
			int32 Size = 1;
			if (b2x2)
			{
				FMCBlockPos Corner;
				if (Find2x2(W, P, B.Id, Corner))
				{
					Base = Corner; Size = 2;
					if (Type == EMCTree::Spruce) Type = EMCTree::MegaSpruce;
					else if (Type == EMCTree::Jungle) Type = EMCTree::MegaJungle;
				}
				else if (bRequire2x2) return false;
			}
			const int32 Height = (Type == EMCTree::MegaJungle || Type == EMCTree::MegaSpruce) ? 14 : (Type == EMCTree::FancyOak ? 10 : 6);
			if (!HasRoom(W, Base, Height, Size)) return false;
			// clear the saplings, then build
			for (int32 a = 0; a < Size; ++a) for (int32 b = 0; b < Size; ++b) W.SetState(FMCBlockPos(Base.X + a, Base.Y + b, Base.Z), 0, MCSet_Render | MCSet_Light | MCSet_NoSupport);
			FMCRandom TreeRand(R.Next());
			RunFeatureInWorld(W, Base, 12, [&](FMCGenWriter& Wr)
			{
				FMCRandom Copy = TreeRand;
				MCFeatures::Tree(Wr, Copy, Type, Base, true);
			});
			// the soil under a tree becomes dirt (podzol for mega spruce)
			for (int32 a = 0; a < Size; ++a) for (int32 b = 0; b < Size; ++b)
			{
				const FMCBlockPos Soil(Base.X + a, Base.Y + b, Base.Z - 1);
				const FMCState SS = W.GetState(Soil);
				if (SS == FMCBlocks::C.Grass || FMCBlocks::GetByState(SS).Model == EMCModel::Farmland) W.SetState(Soil, FMCBlocks::C.Dirt);
			}
			if (Type == EMCTree::MegaSpruce)
				for (int32 i = 0; i < 40; ++i)
				{
					const FMCBlockPos Q(Base.X + R.Range(-4, 5), Base.Y + R.Range(-4, 5), Base.Z - 1);
					for (int32 dz = 2; dz >= -3; --dz)
					{
						const FMCBlockPos QQ = Q.Up(dz);
						const FMCState QS = W.GetState(QQ);
						if ((QS == FMCBlocks::C.Grass || QS == FMCBlocks::C.Dirt) && W.GetState(QQ.Up()) == 0) { W.SetState(QQ, FMCBlocks::C.Podzol); break; }
					}
				}
			return true;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			if (B.Name == TEXT("mangrove_propagule")) return IsSoil(Below) || FMCBlocks::GetByState(Below).Name == TEXT("clay");
			return IsSoil(Below);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (LightAt(W, P.Up()) < 9 || R.NextInt(7) != 0) return;
			Advance(W, P, S, R);
		}
		static void Advance(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R)
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.NumStates > 1 && FMCBlocks::MetaOf(S) == 0) { W.SetState(P, B.State(1), MCSet_None); return; }
			Grow(W, P, S, R);
		}
	};

	// ================================================================================================================
	class FSugarCaneBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			if (FMCBlocks::BlockOf(Below) == B.Id) return true;
			if (!IsSoil(Below) && !IsSandLike(Below) && FMCBlocks::GetByState(Below).Name != TEXT("mud")) return false;
			for (const EMCFace F : { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East })
			{
				const FMCBlockPos Q = P.Down().Offset(F);
				if (MCBehaviorUtil::IsWaterAt(W, Q) || FMCBlocks::GetByState(W.GetState(Q)).Name == TEXT("frosted_ice")) return true;
			}
			return false;
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
			if (W.GetState(P.Up()) != 0) return;
			int32 Height = 1;
			while (FMCBlocks::BlockOf(W.GetState(P.Down(Height))) == B.Id && Height < 4) ++Height;
			if (Height >= 3) return;
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age >= 15) { W.SetState(P.Up(), B.State(0)); W.SetState(P, B.State(0), MCSet_None); }
			else W.SetState(P, B.State((uint8)(Age + 1)), MCSet_None);
		}
	};

	// ================================================================================================================
	class FCactusBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			for (const EMCFace F : { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East })
			{
				const FMCState N = W.GetState(P.Offset(F));
				if (FMCBlocks::IsSolid(N) || FMCBlocks::Is(N, FMCBlocks::C.LavaId)) return false;
			}
			const FMCState Below = W.GetState(P.Down());
			return FMCBlocks::BlockOf(Below) == B.Id || IsSandLike(Below);
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
			if (W.GetState(P.Up()) != 0) return;
			int32 Height = 1;
			while (FMCBlocks::BlockOf(W.GetState(P.Down(Height))) == B.Id && Height < 4) ++Height;
			if (Height >= 3) return;
			const int32 Age = FMCBlocks::MetaOf(S);
			if (Age >= 15)
			{
				W.SetState(P, B.State(0), MCSet_None);
				W.SetState(P.Up(), B.State(0));
			}
			else W.SetState(P, B.State((uint8)(Age + 1)), MCSet_None);
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (!E) return;
			if (E->Kind == EMCEntityKind::Item) { E->Discard(); return; }
			E->Hurt(FMCDamage::Of(TEXT("cactus")), 1.f);
		}
	};

	// ================================================================================================================
	/** Bamboo stalk (meta: bits 0-1 leaves, bit 2 thick) and bamboo shoots. */
	class FBambooBeh : public FMCBlockBehavior
	{
	public:
		static bool IsBamboo(FMCState S) { return FMCBlocks::GetByState(S).Name == TEXT("bamboo"); }
		static bool IsShoot(FMCState S) { return FMCBlocks::GetByState(S).Name == TEXT("bamboo_sapling"); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.BaseState;
			const FMCState Below = C.World->GetState(C.Pos.Down());
			if (IsBamboo(Below)) return B.State(FMCBlocks::MetaOf(Below) & 4);
			if (!IsSoil(Below) && !IsSandLike(Below) && FMCBlocks::GetByState(Below).Name != TEXT("gravel")) return 0;
			// placing bamboo on the ground creates a shoot
			return StateOf(TEXT("bamboo_sapling"));
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return IsBamboo(Below) || IsShoot(Below) || IsSoil(Below) || IsSandLike(Below) || FMCBlocks::GetByState(Below).Name == TEXT("gravel");
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		static bool GrowUp(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R, int32 Steps)
		{
			FMCBlockPos Top = P;
			while (IsBamboo(W.GetState(Top.Up()))) Top = Top.Up();
			int32 Height = 1;
			while (IsBamboo(W.GetState(Top.Down(Height))) && Height < 17) ++Height;
			bool bAny = false;
			for (int32 i = 0; i < Steps; ++i)
			{
				if (Height >= 12 + (int32)(MCHash::Hash2(0xBA, P.X, P.Y) % 5)) break;
				if (W.GetState(Top.Up()) != 0 || LightAt(W, Top.Up()) < 9) break;
				const bool bThick = Height >= 4;
				W.SetState(Top.Up(), StateOf(TEXT("bamboo"), (uint8)((bThick ? 4 : 0) | 2)));
				// leaves: top two large, next small
				W.SetState(Top, StateOf(TEXT("bamboo"), (uint8)((bThick ? 4 : 0) | (Height >= 2 ? 2 : 1))), MCSet_Render);
				if (IsBamboo(W.GetState(Top.Down()))) W.SetState(Top.Down(), StateOf(TEXT("bamboo"), (uint8)((bThick ? 4 : 0) | 1)), MCSet_Render);
				if (IsBamboo(W.GetState(Top.Down(2)))) W.SetState(Top.Down(2), StateOf(TEXT("bamboo"), (uint8)(bThick ? 4 : 0)), MCSet_Render);
				Top = Top.Up();
				++Height;
				bAny = true;
			}
			return bAny;
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (R.NextInt(3) != 0) return;
			if (IsShoot(S))
			{
				if (W.GetState(P.Up()) == 0 && LightAt(W, P.Up()) >= 9) W.SetState(P.Up(), StateOf(TEXT("bamboo"), 1));
				return;
			}
			if (W.GetState(P.Up()) == 0) GrowUp(W, P, S, R, 1);
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("bamboo"), 1));
			return true;
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			// breaking a stalk breaks everything above
			if (IsBamboo(W.GetState(P.Up()))) W.ScheduleTick(P.Up(), FMCBlocks::BlockOf(W.GetState(P.Up())), 1);
		}
	};
}

namespace MCBehPlants
{
	void RegisterPart1()
	{
		Register(EMCBeh::Plant, new FPlantBeh());
		Register(EMCBeh::TallPlant, new FTallPlantBeh());
		Register(EMCBeh::Crop, new FCropBeh());
		Register(EMCBeh::Stem, new FStemBeh());
		Register(EMCBeh::Sapling, new FSaplingBeh());
		Register(EMCBeh::SugarCane, new FSugarCaneBeh());
		Register(EMCBeh::Cactus, new FCactusBeh());
		Register(EMCBeh::Bamboo, new FBambooBeh());
	}

	bool GrowSapling(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) { FSaplingBeh::Advance(W, P, S, R); return true; }
	bool GrowBamboo(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) { return FBambooBeh::GrowUp(W, P, S, R, 1 + R.NextInt(2)); }
}
