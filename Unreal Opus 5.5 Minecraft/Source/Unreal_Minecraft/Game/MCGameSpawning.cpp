// Natural mob spawning: per-category caps, biome / dimension / structure spawn lists, light and surface rules, packs.
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "World/MCWorld.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCBiomes.h"

namespace
{
	struct FSpawnEntry { const TCHAR* Mob; int32 Weight; int32 MinPack; int32 MaxPack; };
	using B = EMCBiome;

	void MonsterList(const FMCBiomeDef& Biome, EMCDimension Dim, FName Structure, int32 Z, TArray<FSpawnEntry>& Out)
	{
		if (Dim == EMCDimension::End) { Out.Add({ TEXT("enderman"), 10, 1, 4 }); return; }
		if (Dim == EMCDimension::Nether)
		{
			if (Structure == TEXT("fortress"))
			{
				Out.Append({ { TEXT("blaze"), 10, 2, 3 }, { TEXT("wither_skeleton"), 8, 1, 4 }, { TEXT("skeleton"), 2, 1, 3 }, { TEXT("magma_cube"), 3, 1, 3 }, { TEXT("zombified_piglin"), 5, 1, 4 } });
				return;
			}
			if (Structure == TEXT("bastion_remnant")) { Out.Append({ { TEXT("piglin"), 10, 2, 4 }, { TEXT("piglin_brute"), 2, 1, 1 }, { TEXT("hoglin"), 3, 1, 2 } }); return; }
			switch (Biome.Id)
			{
			case B::CrimsonForest: Out.Append({ { TEXT("zombified_piglin"), 1, 2, 4 }, { TEXT("hoglin"), 9, 3, 4 }, { TEXT("piglin"), 5, 3, 4 } }); break;
			case B::WarpedForest: Out.Append({ { TEXT("enderman"), 1, 4, 4 } }); break;
			case B::SoulSandValley: Out.Append({ { TEXT("skeleton"), 20, 5, 5 }, { TEXT("ghast"), 50, 4, 4 }, { TEXT("enderman"), 1, 4, 4 } }); break;
			case B::BasaltDeltas: Out.Append({ { TEXT("magma_cube"), 100, 2, 5 }, { TEXT("ghast"), 40, 1, 1 } }); break;
			default: Out.Append({ { TEXT("zombified_piglin"), 100, 4, 4 }, { TEXT("ghast"), 50, 4, 4 }, { TEXT("magma_cube"), 2, 4, 4 }, { TEXT("enderman"), 1, 4, 4 }, { TEXT("piglin"), 15, 4, 4 } }); break;
			}
			return;
		}
		if (Biome.Id == B::MushroomFields || Biome.Id == B::DeepDark) return;
		const bool bDesert = Biome.Id == B::Desert || Biome.Id == B::Badlands || Biome.Id == B::ErodedBadlands || Biome.Id == B::WoodedBadlands;
		const bool bSnowy = Biome.bSnowy || Biome.Temperature < 0.05f;
		const bool bSwamp = Biome.Id == B::Swamp || Biome.Id == B::MangroveSwamp;
		const bool bOcean = Biome.bOcean || Biome.Id == B::River || Biome.Id == B::FrozenRiver;
		Out.Add({ bDesert ? TEXT("husk") : TEXT("zombie"), bDesert ? 80 : 95, 4, 4 });
		if (bDesert) Out.Add({ TEXT("zombie"), 19, 4, 4 });
		Out.Add({ TEXT("zombie_villager"), 5, 1, 1 });
		Out.Add({ bSnowy ? TEXT("stray") : (bSwamp ? TEXT("bogged") : (bDesert ? TEXT("parched") : TEXT("skeleton"))), bSnowy || bDesert ? 80 : (bSwamp ? 50 : 100), 4, 4 });
		if (bSnowy || bSwamp || bDesert) Out.Add({ TEXT("skeleton"), 20, 4, 4 });
		Out.Append({ { TEXT("creeper"), 100, 4, 4 }, { TEXT("spider"), 100, 4, 4 }, { TEXT("enderman"), 10, 1, 4 }, { TEXT("witch"), 5, 1, 1 } });
		if (bOcean) Out.Add({ TEXT("drowned"), 100, 1, 1 });
		if (bSwamp && Z > 50 && Z < 70) Out.Add({ TEXT("slime"), 100, 1, 1 });
		if (Biome.Id == B::LushCaves || Biome.Id == B::DripstoneCaves) Out.Add({ TEXT("zombie"), 40, 2, 4 });
	}

	void CreatureList(const FMCBiomeDef& Biome, TArray<FSpawnEntry>& Out)
	{
		switch (Biome.Id)
		{
		case B::Plains: case B::SunflowerPlains: Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("chicken"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 }, { TEXT("horse"), 5, 2, 6 }, { TEXT("donkey"), 1, 1, 3 } }); break;
		case B::Meadow: Out.Append({ { TEXT("donkey"), 1, 1, 2 }, { TEXT("rabbit"), 2, 2, 6 }, { TEXT("sheep"), 2, 2, 4 } }); break;
		case B::CherryGrove: Out.Append({ { TEXT("pig"), 1, 1, 2 }, { TEXT("rabbit"), 2, 2, 6 } }); break;
		case B::Forest: case B::FlowerForest: case B::BirchForest: case B::OldGrowthBirchForest: case B::DarkForest:
			Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("chicken"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 }, { TEXT("wolf"), Biome.Id == B::Forest ? 5 : 0, 4, 4 }, { TEXT("rabbit"), Biome.Id == B::FlowerForest ? 4 : 0, 2, 3 } }); break;
		case B::Taiga: case B::OldGrowthPineTaiga: case B::OldGrowthSpruceTaiga:
			Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("chicken"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 }, { TEXT("wolf"), 8, 4, 4 }, { TEXT("rabbit"), 4, 2, 3 }, { TEXT("fox"), 8, 2, 4 } }); break;
		case B::SnowyTaiga: case B::Grove: Out.Append({ { TEXT("wolf"), 8, 4, 4 }, { TEXT("rabbit"), 4, 2, 3 }, { TEXT("fox"), 8, 2, 4 } }); break;
		case B::SnowyPlains: case B::IceSpikes: Out.Append({ { TEXT("rabbit"), 10, 2, 3 }, { TEXT("polar_bear"), 1, 1, 2 } }); break;
		case B::SnowySlopes: case B::FrozenPeaks: case B::JaggedPeaks: Out.Append({ { TEXT("goat"), 5, 1, 3 }, { TEXT("rabbit"), 4, 2, 3 } }); break;
		case B::Desert: Out.Append({ { TEXT("rabbit"), 4, 2, 3 }, { TEXT("camel"), 1, 1, 1 } }); break;
		case B::Savanna: case B::SavannaPlateau: case B::WindsweptSavanna:
			Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("chicken"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 }, { TEXT("horse"), 1, 2, 6 }, { TEXT("donkey"), 1, 1, 1 }, { TEXT("llama"), 8, 4, 4 }, { TEXT("armadillo"), 10, 2, 3 } }); break;
		case B::Badlands: case B::ErodedBadlands: case B::WoodedBadlands: Out.Append({ { TEXT("armadillo"), 6, 1, 2 }, { TEXT("cow"), 2, 2, 3 } }); break;
		case B::WindsweptHills: case B::WindsweptGravellyHills: case B::WindsweptForest:
			Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 }, { TEXT("llama"), 5, 4, 6 } }); break;
		case B::Jungle: case B::SparseJungle: case B::BambooJungle:
			Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("chicken"), 10, 4, 4 }, { TEXT("parrot"), 40, 1, 2 }, { TEXT("panda"), Biome.Id == B::BambooJungle ? 80 : 1, 1, 2 }, { TEXT("ocelot"), 2, 1, 3 } }); break;
		case B::Swamp: Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("frog"), 10, 2, 5 } }); break;
		case B::MangroveSwamp: Out.Append({ { TEXT("frog"), 10, 2, 5 } }); break;
		case B::MushroomFields: Out.Append({ { TEXT("mooshroom"), 8, 4, 8 } }); break;
		case B::Beach: Out.Append({ { TEXT("turtle"), 5, 2, 5 } }); break;
		case B::River: case B::FrozenRiver: break;
		case B::PaleGarden: break;
		default: if (!Biome.bOcean && !Biome.bCave && !Biome.bNether && !Biome.bEnd) Out.Append({ { TEXT("sheep"), 12, 4, 4 }, { TEXT("pig"), 10, 4, 4 }, { TEXT("cow"), 8, 4, 4 } }); break;
		}
	}

	void WaterList(const FMCBiomeDef& Biome, TArray<FSpawnEntry>& Out)
	{
		const B Id = Biome.Id;
		if (Id == B::WarmOcean) Out.Append({ { TEXT("tropical_fish"), 25, 8, 8 }, { TEXT("pufferfish"), 15, 1, 3 }, { TEXT("dolphin"), 2, 1, 2 } });
		else if (Id == B::LukewarmOcean || Id == B::DeepLukewarmOcean) Out.Append({ { TEXT("tropical_fish"), 25, 8, 8 }, { TEXT("pufferfish"), 5, 1, 3 }, { TEXT("cod"), 15, 3, 6 }, { TEXT("squid"), 10, 1, 4 }, { TEXT("dolphin"), 2, 1, 2 }, { TEXT("nautilus"), 1, 1, 1 } });
		else if (Id == B::ColdOcean || Id == B::DeepColdOcean || Id == B::FrozenOcean || Id == B::DeepFrozenOcean) Out.Append({ { TEXT("cod"), 15, 3, 6 }, { TEXT("salmon"), 15, 1, 5 }, { TEXT("squid"), 3, 1, 4 } });
		else if (Id == B::River || Id == B::FrozenRiver) Out.Append({ { TEXT("salmon"), 5, 1, 5 }, { TEXT("squid"), 2, 1, 4 } });
		else if (Biome.bOcean) Out.Append({ { TEXT("cod"), 10, 3, 6 }, { TEXT("squid"), 10, 1, 4 }, { TEXT("dolphin"), 1, 1, 2 } });
		else if (Id == B::LushCaves) Out.Append({ { TEXT("axolotl"), 10, 4, 6 }, { TEXT("tropical_fish"), 25, 8, 8 } });
		else if (Id == B::Swamp || Id == B::MangroveSwamp) Out.Append({ { TEXT("tadpole"), 2, 1, 3 } });
	}

	const FSpawnEntry* Pick(const TArray<FSpawnEntry>& L, FMCRandom& R)
	{
		int32 Total = 0;
		for (const FSpawnEntry& E : L) Total += E.Weight;
		if (Total <= 0) return nullptr;
		int32 Roll = R.NextInt(Total);
		for (const FSpawnEntry& E : L) { Roll -= E.Weight; if (Roll < 0) return &E; }
		return nullptr;
	}

	bool IsSlimeChunk(uint64 Seed, int32 CX, int32 CY)
	{
		return MCHash::Hash2(Seed ^ 0x3AD8025Full, CX, CY) % 10 == 0;
	}
}

bool AMCGame::CanSpawnAt(FMCWorld& W, FName Mob, const FMCBlockPos& P, bool bHostile) const
{
	const FMCMobDef* Def = MCMobs::Find(Mob);
	if (!Def || !W.IsReadyAt(P) || !FMCChunk::InRange(P.Z)) return false;
	const FMCState At = W.GetState(P), Above = W.GetState(P.Up()), Below = W.GetState(P.Down());
	const bool bWaterMob = Def->bSwimmer && !Def->bAmphibious;
	if (bWaterMob || Mob == TEXT("drowned") || Mob == TEXT("axolotl") || Mob == TEXT("glow_squid"))
	{
		const bool bWater = FMCBlocks::Info(At).Block == FMCBlocks::C.WaterId && FMCBlocks::Info(Above).Block == FMCBlocks::C.WaterId;
		if (!bWater) return false;
		if (Mob == TEXT("drowned")) return W.GetBlockLight(P) == 0;
		if (Mob == TEXT("axolotl") || Mob == TEXT("glow_squid")) return P.Z < 30 && FMCBlocks::IsSolid(Below);
		return true;
	}
	if (Mob == TEXT("strider")) return FMCBlocks::Info(Below).Block == FMCBlocks::C.LavaId && At == 0;
	// room for the mob
	const FMCBox Box(P.X + 0.5 - Def->Width * 0.5, P.Y + 0.5 - Def->Width * 0.5, P.Z, P.X + 0.5 + Def->Width * 0.5, P.Y + 0.5 + Def->Width * 0.5, P.Z + Def->Height);
	if (!W.IsRegionFree(Box) || W.IsInFluid(Box, FMCBlocks::C.WaterId) || W.IsInFluid(Box, FMCBlocks::C.LavaId)) return false;
	const FMCStateInfo& BI = FMCBlocks::Info(Below);
	if (!(BI.Flags & MCB_Solid) || !BI.bFullCollision) return false;
	const FName BN = FMCBlocks::Get(BI.Block).Name;
	if (BN == TEXT("bedrock") || BN == TEXT("barrier") || (BN == TEXT("magma_block") && Mob != TEXT("magma_cube") && Mob != TEXT("strider")) || (BI.Flags & MCB_Leaves) || BN.ToString().Contains(TEXT("glass"))) return false;
	if (bHostile)
	{
		if (W.Dim == EMCDimension::Overworld)
		{
			if (W.GetBlockLight(P) > 0) return false;
			const int32 Sky = (int32)W.GetSkyLight(P) - GetSkyDarken();
			if (Sky > W.Rand.NextInt(8)) return false;
			if (Mob == TEXT("slime"))
			{
				const bool bSwamp = FMCBiomes::Get(W.GetBiome(P)).Id == EMCBiome::Swamp;
				if (!(bSwamp && P.Z > 50 && P.Z < 70) && !(IsSlimeChunk(Seed, P.ChunkX(), P.ChunkY()) && P.Z < 40)) return false;
			}
		}
		else if (W.Dim == EMCDimension::Nether)
		{
			if (W.GetBlockLight(P) > 11) return false;
			if (Mob == TEXT("ghast"))
			{
				const FMCBox Big(P.X - 1.5, P.Y - 1.5, P.Z, P.X + 2.5, P.Y + 2.5, P.Z + 4);
				if (!W.IsRegionFree(Big)) return false;
			}
		}
		return true;
	}
	// animals: daylight on grass-like ground
	if (W.GetLight(P, GetSkyDarken()) < 9 && W.GetSkyLight(P) < 9) return false;
	if (Mob == TEXT("turtle") || Mob == TEXT("camel") || (Mob == TEXT("rabbit") && BN == TEXT("sand"))) return (BI.Flags & MCB_Sand) != 0 || BN == TEXT("sand");
	if (Mob == TEXT("mooshroom")) return BN == TEXT("mycelium");
	if (Mob == TEXT("goat")) return BN == TEXT("stone") || BN == TEXT("snow_block") || BN == TEXT("powder_snow") || BN == TEXT("grass_block") || BN == TEXT("packed_ice");
	if (Mob == TEXT("polar_bear") || Mob == TEXT("fox") || Mob == TEXT("rabbit") || Mob == TEXT("wolf")) return BN == TEXT("grass_block") || BN == TEXT("snow_block") || BN == TEXT("podzol") || BN == TEXT("coarse_dirt") || BN == TEXT("ice") || BN == TEXT("packed_ice") || FMCBlocks::GetByState(At).Name == TEXT("snow");
	if (Mob == TEXT("parrot")) return BN == TEXT("grass_block") || (BI.Flags & MCB_Log) || (BI.Flags & MCB_Leaves);
	if (Mob == TEXT("armadillo")) return BN == TEXT("grass_block") || BN == TEXT("red_sand") || BN == TEXT("coarse_dirt") || BN.ToString().Contains(TEXT("terracotta"));
	if (Mob == TEXT("frog")) return BN == TEXT("grass_block") || BN == TEXT("mud") || BN == TEXT("mangrove_roots") || BN == TEXT("muddy_mangrove_roots");
	return BN == TEXT("grass_block");
}

void AMCGame::TickSpawning(FMCWorld& W)
{
	if (!Player || Player->World != &W) return;
	++SpawnTimer;
	// counts per category
	int32 Monsters = 0, Creatures = 0, Ambient = 0, Water = 0, WaterAmbient = 0, Underground = 0;
	for (const AMCEntity* E : W.Entities)
	{
		const AMCMob* M = Cast<AMCMob>(E);
		if (!M || M->bRemoved || !M->Def) continue;
		switch (M->Def->Category)
		{
		case EMCMobCategory::Monster: ++Monsters; break;
		case EMCMobCategory::Creature: ++Creatures; break;
		case EMCMobCategory::Ambient: ++Ambient; break;
		case EMCMobCategory::WaterCreature: ++Water; break;
		case EMCMobCategory::WaterAmbient: ++WaterAmbient; break;
		case EMCMobCategory::Underground: ++Underground; break;
		default: break;
		}
	}
	const int32 SimChunks = FMath::Square(SimulationDistance * 2 + 1);
	const float Scale = SimChunks / 289.f;
	const bool bMonsters = Difficulty != EMCDifficulty::Peaceful && Monsters < FMath::CeilToInt(70 * Scale);
	const bool bCreatures = SpawnTimer % 400 == 0 && Creatures < FMath::CeilToInt(10 * Scale);
	const bool bAmbient = SpawnTimer % 20 == 0 && Ambient < FMath::CeilToInt(15 * Scale);
	const bool bWater = SpawnTimer % 20 == 5 && Water < FMath::CeilToInt(5 * Scale);
	const bool bWaterAmbient = SpawnTimer % 20 == 10 && WaterAmbient < FMath::CeilToInt(20 * Scale);
	const bool bUnder = SpawnTimer % 40 == 15 && Underground < FMath::CeilToInt(5 * Scale);
	if (!bMonsters && !bCreatures && !bAmbient && !bWater && !bWaterAmbient && !bUnder) return;

	FMCRandom& R = W.Rand;
	const FMCChunkPos PC = FMCChunkPos::FromBlock(Player->BlockPos());
	auto TrySpawnIn = [&](int32 Category)
	{
		const FMCChunkPos C(PC.X + R.Range(-SimulationDistance, SimulationDistance), PC.Y + R.Range(-SimulationDistance, SimulationDistance));
		const FMCChunk* Ch = W.GetChunk(C);
		if (!Ch || Ch->Stage != EMCChunkStage::Lit) return;
		const int32 X = C.MinBlockX() + R.NextInt(16), Y = C.MinBlockY() + R.NextInt(16);
		const int32 Top = Ch->GetMotionHeight(X & 15, Y & 15) + 1;
		int32 Z;
		if (Category == 1 || Category == 5) Z = Top; // creatures on the surface
		else Z = R.Range(W.Dim == EMCDimension::Nether ? 1 : MC::MinZ, FMath::Max(MC::MinZ + 1, Top));
		const FMCBlockPos P0(X, Y, Z);
		const FMCBiomeDef& Biome = FMCBiomes::Get(W.GetBiome(P0));
		const FName Structure = W.Generator ? W.Generator->GetStructureAt(P0) : NAME_None;
		TArray<FSpawnEntry> List;
		switch (Category)
		{
		case 0: MonsterList(Biome, W.Dim, Structure, Z, List); break;
		case 1: if (W.Dim == EMCDimension::Overworld) CreatureList(Biome, List); else if (W.Dim == EMCDimension::Nether && Biome.Id != EMCBiome::BasaltDeltas) List.Add({ TEXT("strider"), 60, 1, 2 }); break;
		case 2: if (W.Dim == EMCDimension::Overworld && Z < 63) List.Add({ TEXT("bat"), 10, 8, 8 }); break;
		case 3: case 4: if (W.Dim == EMCDimension::Overworld) WaterList(Biome, List); break;
		case 5:
			if (W.Dim == EMCDimension::Overworld && Biome.Id == EMCBiome::SulfurCaves) List.Add({ TEXT("sulfur_cube"), 10, 1, 2 });
			if (W.Dim == EMCDimension::Overworld && Z < 30) List.Add({ TEXT("glow_squid"), 10, 2, 4 });
			break;
		default: break;
		}
		if (Category == 3) List.RemoveAll([](const FSpawnEntry& E) { const FMCMobDef* D = MCMobs::Find(E.Mob); return !D || D->Category != EMCMobCategory::WaterCreature; });
		if (Category == 4) List.RemoveAll([](const FSpawnEntry& E) { const FMCMobDef* D = MCMobs::Find(E.Mob); return !D || D->Category != EMCMobCategory::WaterAmbient; });
		const FSpawnEntry* E = Pick(List, R);
		if (!E) return;
		const int32 Pack = R.Range(E->MinPack, E->MaxPack);
		int32 Spawned = 0;
		FMCBlockPos P = P0;
		for (int32 i = 0; i < Pack * 3 && Spawned < Pack; ++i)
		{
			P = FMCBlockPos(P.X + R.Range(-5, 5), P.Y + R.Range(-5, 5), P.Z);
			if (Category == 1)
			{
				const FMCChunk* PCh = W.GetChunkAt(P);
				if (!PCh) continue;
				P.Z = PCh->GetMotionHeight(P.X & 15, P.Y & 15) + 1;
			}
			const double DSq = FVector::DistSquared(FVector(P.X + 0.5, P.Y + 0.5, P.Z), Player->Pos);
			if (DSq < 24.0 * 24.0 || DSq > 128.0 * 128.0) continue;
			if (FVector::DistSquared(FVector(P.X, P.Y, P.Z), FVector(WorldSpawn.X, WorldSpawn.Y, WorldSpawn.Z)) < 24.0 * 24.0 && Category == 0 && W.Dim == EMCDimension::Overworld) continue;
			if (!CanSpawnAt(W, FName(E->Mob), P, Category == 0)) continue;
			if (AMCEntity* M = SpawnMob(&W, FName(E->Mob), FVector(P.X + 0.5, P.Y + 0.5, P.Z), true))
			{
				++Spawned;
				// jockeys, babies and variants
				if (AMCMob* Mob = Cast<AMCMob>(M))
				{
					const FName Id = Mob->Def->Id;
					if ((Id == TEXT("zombie") || Id == TEXT("husk") || Id == TEXT("drowned") || Id == TEXT("zombie_villager")) && R.NextInt(100) < 5) Mob->SetBaby(true);
					if (Category == 1 && R.NextInt(10) == 0) Mob->SetBaby(true);
					if (Id == TEXT("spider") && R.NextInt(100) == 0) if (AMCMob* J = Cast<AMCMob>(SpawnMob(&W, TEXT("skeleton"), Mob->Pos, true))) J->StartRiding(Mob);
				}
			}
		}
	};
	if (bMonsters) for (int32 i = 0; i < 3; ++i) TrySpawnIn(0);
	if (bCreatures) for (int32 i = 0; i < 4; ++i) TrySpawnIn(1);
	if (bAmbient) TrySpawnIn(2);
	if (bWater) TrySpawnIn(3);
	if (bWaterAmbient) TrySpawnIn(4);
	if (bUnder) TrySpawnIn(5);

	// wandering traders visit occasionally
	if (SpawnTimer % 24000 == 12000 && W.Dim == EMCDimension::Overworld && R.NextInt(10) < 3)
	{
		const FMCBlockPos PP = Player->BlockPos();
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const int32 X = PP.X + R.Range(-40, 40), Y = PP.Y + R.Range(-40, 40);
			if (!W.IsReadyAt(FMCBlockPos(X, Y, 64))) continue;
			const FMCBlockPos P(X, Y, W.GetTopSolidZ(X, Y) + 1);
			if (!CanSpawnAt(W, TEXT("wandering_trader"), P, false) && !W.IsRegionFree(FMCBox(FVector(X, Y, P.Z), FVector(X + 1, Y + 1, P.Z + 2)))) continue;
			if (AMCEntity* T = SpawnMob(&W, TEXT("wandering_trader"), FVector(X + 0.5, Y + 0.5, P.Z), true))
			{
				for (int32 l = 0; l < 2; ++l) if (AMCMob* L = Cast<AMCMob>(SpawnMob(&W, TEXT("trader_llama"), T->Pos + FVector(1 + l, 0, 0), true))) { L->LeashHolder = T; }
				AddChat(TEXT("A wandering trader has arrived nearby"));
			}
			break;
		}
	}
}
