// FMCWorld: block destruction, explosions, entity registry & persistence, and effect forwarding to the game.
#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"
#include "Blocks/MCBlockBehavior.h"
#include "Blocks/MCBehaviorsInternal.h"
#include "Items/MCLoot.h"
#include "Game/MCGame.h"
#include "Game/MCEntity.h"
#include "Game/MCLiving.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
	const TCHAR* SoundGroupName(EMCSound S)
	{
		static const TCHAR* Names[] = {
			TEXT("stone"), TEXT("wood"), TEXT("gravel"), TEXT("grass"), TEXT("sand"), TEXT("snow"), TEXT("glass"), TEXT("wool"), TEXT("metal"),
			TEXT("slime"), TEXT("honey"), TEXT("netherrack"), TEXT("nylium"), TEXT("bone"), TEXT("soul_sand"), TEXT("amethyst"), TEXT("sculk"),
			TEXT("deepslate"), TEXT("mud"), TEXT("copper"), TEXT("chain"), TEXT("lantern"), TEXT("ladder"), TEXT("scaffolding"), TEXT("crop"),
			TEXT("coral"), TEXT("wart"), TEXT("fungus"), TEXT("basalt"), TEXT("cloth"), TEXT("candle"), TEXT("moss"), TEXT("froglight"),
			TEXT("lodestone"), TEXT("bamboo"), TEXT("cherry") };
		const int32 I = (int32)S;
		return I >= 0 && I < (int32)UE_ARRAY_COUNT(Names) ? Names[I] : TEXT("stone");
	}

	/** Pre-built "block_<group>_<kind>" names (avoids FName construction per footstep). */
	FName BlockSoundName(EMCSound S, int32 Kind)
	{
		static TArray<FName> Cache;
		static const TCHAR* Kinds[4] = { TEXT("break"), TEXT("place"), TEXT("step"), TEXT("hit") };
		const int32 NumGroups = (int32)EMCSound::Count;
		if (Cache.Num() == 0)
		{
			Cache.SetNum(NumGroups * 4);
			for (int32 g = 0; g < NumGroups; ++g)
				for (int32 k = 0; k < 4; ++k)
					Cache[g * 4 + k] = FName(*FString::Printf(TEXT("block_%s_%s"), SoundGroupName((EMCSound)g), Kinds[k]));
		}
		return Cache[FMath::Clamp((int32)S, 0, NumGroups - 1) * 4 + FMath::Clamp(Kind, 0, 3)];
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Block destruction

bool FMCWorld::DestroyBlock(const FMCBlockPos& P, bool bDrop, AMCEntity* Breaker, const FMCItemStack* Tool, bool bEffects)
{
	const FMCState S = GetState(P);
	if (S == 0) return false;
	const FMCStateInfo& I = FMCBlocks::Info(S);
	const FMCBlock& B = FMCBlocks::Get(I.Block);
	if (bEffects && !(I.Flags & MCB_Fluid))
	{
		SpawnBlockBreakParticles(P, S);
		PlayBlockSound(S, 0, FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5));
	}
	AMCPlayer* PlayerBreaker = Cast<AMCPlayer>(Breaker);
	const bool bCreative = PlayerBreaker && PlayerBreaker->IsCreative();
	if (bDrop && !bCreative)
	{
		if (FMCBlockEntity* BE = GetBlockEntity(P)) BE->DropContents(*this);
		MCBeh::SpawnDrops(*this, P, S, Tool, Breaker);
		// experience from ores / sculk / spawners (not with silk touch)
		if (B.XPMax > 0.f && PlayerBreaker && !(Tool && Tool->GetEnchant(EMCEnchant::SilkTouch) > 0))
		{
			const int32 XP = FMath::RoundToInt(FMath::Lerp(B.XPMin, B.XPMax, Rand.NextFloat()));
			if (XP > 0) SpawnXP(FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), XP);
		}
	}
	else if (bCreative)
	{
		// creative breaking never drops, but containers still lose their contents silently
	}
	const FMCState Replacement = (I.Flags & MCB_Waterlogged) && !(I.Flags & MCB_Fluid) ? FMCBlocks::C.Water : 0;
	SetState(P, Replacement, MCSet_Default);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Entities

void FMCWorld::RegisterEntity(AMCEntity* E)
{
	if (!E) return;
	E->World = this;
	Entities.AddUnique(E);
}

void FMCWorld::UnregisterEntity(AMCEntity* E)
{
	Entities.RemoveSingleSwap(E, EAllowShrinking::No);
}

void FMCWorld::GetEntitiesInBox(const FMCBox& Box, TArray<AMCEntity*>& Out, const AMCEntity* Except) const
{
	for (AMCEntity* E : Entities)
	{
		if (!E || E == Except || E->bRemoved) continue;
		if (E->GetBox().Intersects(Box)) Out.Add(E);
	}
}

AMCEntity* FMCWorld::SpawnMob(FName MobId, const FVector& PosBlocks, bool bNatural, int32 Variant)
{
	return Game ? Game->SpawnMob(this, MobId, PosBlocks, bNatural, Variant) : nullptr;
}

void FMCWorld::SpawnItem(const FVector& PosBlocks, const FMCItemStack& Stack, bool bRandomVelocity, float PickupDelay)
{
	if (!Game || Stack.IsEmpty()) return;
	AMCItemEntity* IE = Game->SpawnEntity<AMCItemEntity>(this, PosBlocks);
	if (!IE) return;
	IE->SetStack(Stack);
	IE->PickupDelay = FMath::RoundToInt(PickupDelay * MC::TicksPerSecond);
	if (bRandomVelocity)
	{
		IE->Vel = FVector(Rand.FRange(-0.1f, 0.1f), Rand.FRange(-0.1f, 0.1f), 0.2);
	}
}

void FMCWorld::SpawnXP(const FVector& PosBlocks, int32 Amount)
{
	if (!Game || Amount <= 0) return;
	while (Amount > 0)
	{
		const int32 V = AMCXPOrb::SplitValue(Amount);
		Amount -= V;
		AMCXPOrb* O = Game->SpawnEntity<AMCXPOrb>(this, PosBlocks + FVector(Rand.FRange(-0.3f, 0.3f), Rand.FRange(-0.3f, 0.3f), 0.1));
		if (!O) return;
		O->Value = V;
		O->Vel = FVector(Rand.FRange(-0.1f, 0.1f), Rand.FRange(-0.1f, 0.1f), Rand.FRange(0.05f, 0.25f));
	}
}

int32 FMCWorld::CountMobs(bool bHostile) const
{
	int32 N = 0;
	for (const AMCEntity* E : Entities)
	{
		const AMCMob* M = Cast<AMCMob>(E);
		if (!M || M->bRemoved || !M->Def) continue;
		const bool bMonster = M->Def->Category == EMCMobCategory::Monster;
		if (bMonster == bHostile) ++N;
	}
	return N;
}

// ---------------------------------------------------------------------------------------------------------------------
// Persistence of entities with their chunk

void FMCWorld::CaptureEntities(FMCChunk& C, bool bRemove)
{
	TArray<AMCEntity*> Inside;
	for (AMCEntity* E : Entities)
	{
		if (!E || E->bRemoved || !E->ShouldSave() || E->IsA<AMCPlayer>()) continue;
		if (E->IsPassenger() && E->Vehicle.IsValid() && E->Vehicle->IsA<AMCPlayer>()) continue;
		if (FMCChunkPos::FromBlock(MC::FloorToInt(E->Pos.X), MC::FloorToInt(E->Pos.Y)) != C.Pos) continue;
		// boss fights are kept alive by the game and are not written with terrain
		if (E->IsA<AMCEnderDragon>()) continue;
		Inside.Add(E);
	}
	const bool bHadEntities = C.SavedEntities.Num() > 0;
	if (Inside.Num() == 0)
	{
		if (bHadEntities) { C.SavedEntities.Reset(); C.bModified = true; }
		return;
	}
	FBufferArchive Ar;
	int32 Count = Inside.Num();
	Ar << Count;
	for (AMCEntity* E : Inside)
	{
		FString ClassName = E->GetClass()->GetName();
		FName Type = E->TypeId;
		Ar << ClassName << Type;
		FBufferArchive Body;
		E->Serialize(Body);
		TArray<uint8> Bytes(Body);
		Ar << Bytes;
	}
	C.SavedEntities = TArray<uint8>(Ar);
	C.bModified = true;
	if (bRemove)
	{
		for (AMCEntity* E : Inside) if (Game) Game->DestroyEntity(E);
	}
}

void FMCWorld::RestoreEntities(FMCChunk& C)
{
	if (C.SavedEntities.Num() == 0 || !Game) return;
	TArray<uint8> Data = MoveTemp(C.SavedEntities);
	C.SavedEntities.Reset();
	FMemoryReader Ar(Data);
	int32 Count = 0;
	Ar << Count;
	for (int32 i = 0; i < Count && !Ar.IsError(); ++i)
	{
		FString ClassName;
		FName Type;
		TArray<uint8> Bytes;
		Ar << ClassName << Type << Bytes;
		if (Ar.IsError()) break;
		UClass* Cls = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Unreal_Minecraft.%s"), *ClassName));
		if (!Cls || !Cls->IsChildOf(AMCEntity::StaticClass())) continue;
		// read the position first so the entity spawns in place
		AMCEntity* E = nullptr;
		if (Cls->IsChildOf(AMCMob::StaticClass()) && !Type.IsNone())
		{
			E = Game->SpawnMob(this, Type, FVector(C.Pos.MinBlockX() + 8, C.Pos.MinBlockY() + 8, 100), false, -1);
		}
		else
		{
			E = Game->SpawnEntityOfClass(Cls, this, FVector(C.Pos.MinBlockX() + 8, C.Pos.MinBlockY() + 8, 100));
		}
		if (!E) continue;
		if (!E->IsA<AMCMob>()) E->InitEntity();
		FMemoryReader Body(Bytes);
		E->Serialize(Body);
		E->SetPosition(E->Pos, true);
	}
	C.bModified = true; // entities now live in the world; they are re-captured on unload
}

void FMCWorld::ProcessPendingSpawns(FMCChunk& C)
{
	if (!Game) return;
	if (!C.bSpawnsProcessed)
	{
		C.bSpawnsProcessed = true;
		for (const FMCPendingSpawn& S : C.PendingSpawns)
		{
			AMCEntity* E = SpawnMob(S.Mob, S.Pos, false, S.Variant);
			if (!E)
			{
				// non-mob generation entities (end crystals, item frames...)
				if (S.Mob == TEXT("end_crystal"))
				{
					if (AMCEndCrystal* EC = Game->SpawnEntity<AMCEndCrystal>(this, S.Pos))
					{
						EC->bSpike = true;
						EC->bShowBottom = true;
					}
				}
				continue;
			}
			E->bPersistent = S.bPersistent;
			if (AMCMob* M = Cast<AMCMob>(E))
			{
				if (S.Extra == TEXT("baby")) M->SetBaby(true);
				else if (!S.Extra.IsNone()) M->Profession = S.Extra;
			}
		}
		C.PendingSpawns.Reset();
	}
	RestoreEntities(C);
}

// ---------------------------------------------------------------------------------------------------------------------
// Explosions (ray casting like Minecraft: 16x16x16 grid surface rays)

void FMCWorld::Explode(const FVector& Center, float Power, bool bFire, bool bBreakBlocks, AActor* Source)
{
	AMCEntity* SourceEntity = Cast<AMCEntity>(Source);
	const bool bGriefing = !Game || Game->Rules.bMobGriefing || !SourceEntity || SourceEntity->IsA<AMCPrimedTNT>() || SourceEntity->IsA<AMCMinecart>() || SourceEntity->IsA<AMCEndCrystal>();
	TSet<FMCBlockPos> Affected;
	if (bBreakBlocks && bGriefing)
	{
		for (int32 j = 0; j < 16; ++j)
			for (int32 k = 0; k < 16; ++k)
				for (int32 l = 0; l < 16; ++l)
				{
					if (j != 0 && j != 15 && k != 0 && k != 15 && l != 0 && l != 15) continue;
					FVector D((j / 15.0) * 2.0 - 1.0, (k / 15.0) * 2.0 - 1.0, (l / 15.0) * 2.0 - 1.0);
					D.Normalize();
					double Strength = Power * (0.7 + Rand.NextDouble() * 0.6);
					FVector P = Center;
					while (Strength > 0.0)
					{
						const FMCBlockPos BP(MC::FloorToInt(P.X), MC::FloorToInt(P.Y), MC::FloorToInt(P.Z));
						const FMCState S = GetState(BP);
						if (!FMCChunk::InRange(BP.Z)) break;
						if (S != 0)
						{
							const FMCBlock& B = FMCBlocks::GetByState(S);
							const bool bFluid = FMCBlocks::IsFluid(S);
							const float Res = B.Has(MCB_Unbreakable) || B.Hardness < 0.f ? 3600000.f : (bFluid ? 100.f : B.BlastResistance);
							Strength -= (Res + 0.3) * 0.3;
							if (Strength > 0.0 && !bFluid) Affected.Add(BP);
						}
						P += D * 0.3;
						Strength -= 0.22500001;
					}
				}
	}

	// entities
	const double R = Power * 2.0;
	TArray<AMCEntity*> Near;
	GetEntitiesInBox(FMCBox(Center - FVector(R + 1), Center + FVector(R + 1)), Near, nullptr);
	for (AMCEntity* E : Near)
	{
		if (!E || E->bRemoved || E == SourceEntity) continue;
		const double Dist = FVector::Dist(E->Pos + FVector(0, 0, E->Height * 0.5), Center) / R;
		if (Dist > 1.0) continue;
		// exposure: fraction of sample rays from the centre that reach the entity box unobstructed
		const FMCBox Box = E->GetBox();
		int32 Hits = 0, Samples = 0;
		for (double fx = 0; fx <= 1.0; fx += 0.5)
			for (double fy = 0; fy <= 1.0; fy += 0.5)
				for (double fz = 0; fz <= 1.0; fz += 0.5)
				{
					const FVector T(FMath::Lerp(Box.Min.X, Box.Max.X, fx), FMath::Lerp(Box.Min.Y, Box.Max.Y, fy), FMath::Lerp(Box.Min.Z, Box.Max.Z, fz));
					FVector Dir = T - Center;
					const double Len = Dir.Size();
					++Samples;
					if (Len < 1e-3) { ++Hits; continue; }
					Dir /= Len;
					FMCRayHit Hit;
					if (!Raycast(Center, Dir, Len, Hit, false, false)) ++Hits;
				}
		const double Exposure = Samples > 0 ? (double)Hits / Samples : 1.0;
		const double Impact = (1.0 - Dist) * Exposure;
		const float Damage = (float)FMath::FloorToDouble((Impact * Impact + Impact) / 2.0 * 7.0 * R + 1.0);
		FMCDamage D = FMCDamage::Explosion(Center, SourceEntity);
		if (AMCPrimedTNT* TNT = Cast<AMCPrimedTNT>(SourceEntity)) D.Attacker = TNT->Igniter;
		if (E->IsA<AMCItemEntity>() || E->IsA<AMCXPOrb>())
		{
			E->Hurt(D, Damage);
		}
		else
		{
			E->Hurt(D, Damage);
		}
		FVector Push = (E->Pos + FVector(0, 0, E->EyeHeight)) - Center;
		if (!Push.IsNearlyZero())
		{
			Push.Normalize();
			double KB = Impact;
			if (AMCLiving* L = Cast<AMCLiving>(E))
			{
				const int32 BlastProt = L->GetEnchantMax(EMCEnchant::BlastProtection);
				if (BlastProt > 0) KB *= FMath::Max(0.0, 1.0 - BlastProt * 0.15);
				KB *= 1.0 - L->KnockbackResistance;
			}
			AMCPlayer* PL = Cast<AMCPlayer>(E);
			if (!(PL && (PL->IsCreative() && PL->bFlying)))
			{
				E->Vel += Push * KB;
			}
		}
	}

	// blocks: behaviour hook, drops (1 / power chance), removal
	TArray<FMCBlockPos> Order = Affected.Array();
	Order.Sort([&](const FMCBlockPos& A, const FMCBlockPos& B) { return A.Z > B.Z; });
	const float DropChance = SourceEntity && SourceEntity->IsA<AMCPrimedTNT>() ? 1.f / Power : 1.f / Power;
	for (const FMCBlockPos& BP : Order)
	{
		const FMCState S = GetState(BP);
		if (S == 0) continue;
		const FMCBlock& B = FMCBlocks::GetByState(S);
		B.Behavior->OnExploded(*this, BP, S, SourceEntity);
		if (GetState(BP) != S) continue; // behaviour replaced it (TNT primed)
		if (Rand.Chance(DropChance) && (!Game || Game->Rules.bDoTileDrops))
		{
			if (FMCBlockEntity* BE = GetBlockEntity(BP)) BE->DropContents(*this);
			TArray<FMCItemStack> Drops;
			MCLoot::GetBlockDrops(*this, BP, S, nullptr, Drops, Rand);
			for (const FMCItemStack& D : Drops) MCBehaviorUtil::PopItem(*this, BP, D);
		}
		else if (FMCBlockEntity* BE = GetBlockEntity(BP))
		{
			BE->DropContents(*this);
		}
		SetState(BP, (B.Flags & MCB_Waterlogged) ? FMCBlocks::C.Water : 0, MCSet_Default);
	}
	if (bFire)
	{
		for (const FMCBlockPos& BP : Order)
		{
			if (Rand.NextInt(3) != 0) continue;
			if (GetState(BP) == 0 && FMCBlocks::IsSolid(GetState(BP.Down())) && !FMCBlocks::IsFluid(GetState(BP.Down())))
			{
				SetState(BP, FMCBlocks::C.Fire, MCSet_Default);
			}
		}
	}

	// effects
	PlaySound(TEXT("explode"), Center, 4.f, (1.f + (Rand.NextFloat() - Rand.NextFloat()) * 0.2f) * 0.7f);
	SpawnParticles(Power >= 2.f && bBreakBlocks ? TEXT("explosion_emitter") : TEXT("explosion"), Center, Power >= 2.f ? 1 : 3, Power * 0.5f);
	SpawnParticles(TEXT("smoke_large"), Center, 24, Power * 0.6f, FVector(0, 0, 0.6), FColor(90, 90, 90));
	if (Game)
	{
		if (AMCPlayer* P = Game->Player)
		{
			const double D = FVector::Dist(P->Pos, Center);
			if (D < Power * 6.0) Game->ScreenShake((float)(1.0 - D / (Power * 6.0)) * 0.8f);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Effects (only audible / visible for the active dimension)

void FMCWorld::PlaySound(FName Sound, const FVector& PosBlocks, float Volume, float Pitch) const
{
	if (Game && Game->ActiveWorld() == this) Game->PlaySound(const_cast<FMCWorld*>(this), Sound, PosBlocks, Volume, Pitch);
}

void FMCWorld::PlayBlockSound(FMCState S, int32 Kind, const FVector& PosBlocks) const
{
	if (!Game || Game->ActiveWorld() != this || S == 0) return;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	const float Volume = Kind == 2 ? 0.15f : (Kind == 3 ? 0.25f : 1.f);
	const float Pitch = Kind == 3 ? 0.5f : (Kind == 2 ? 1.f : 0.8f);
	Game->PlaySound(const_cast<FMCWorld*>(this), BlockSoundName(B.Sound, Kind), PosBlocks, Volume, Pitch * (0.9f + 0.2f * FMath::FRand()));
}

void FMCWorld::SpawnBlockBreakParticles(const FMCBlockPos& P, FMCState S) const
{
	if (Game && Game->ActiveWorld() == this) Game->SpawnBlockParticles(const_cast<FMCWorld*>(this), P, S, true);
}

void FMCWorld::SpawnParticles(FName Type, const FVector& PosBlocks, int32 Count, float Spread, const FVector& Vel, FColor Color) const
{
	if (Game && Game->ActiveWorld() == this) Game->SpawnParticles(const_cast<FMCWorld*>(this), Type, PosBlocks, Count, Spread, Vel, Color);
}
