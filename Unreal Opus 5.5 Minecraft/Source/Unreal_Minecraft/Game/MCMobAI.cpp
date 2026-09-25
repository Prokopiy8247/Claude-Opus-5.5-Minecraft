// Mob AI archetypes (part 1): targeting, animals, zombies, skeletons, creepers, spiders, endermen, slimes, ghasts, blazes.
#include "Game/MCMob.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCEntities.h"
#include "Items/MCLoot.h"
#include "World/MCWorld.h"

namespace
{
	bool IsValidTarget(const AMCLiving* T, const AMCMob* Self)
	{
		if (!T || !T->IsAlive() || T->bRemoved || T->World != Self->World) return false;
		if (const AMCPlayer* P = Cast<AMCPlayer>(T)) if (P->IsCreative() || P->IsSpectator()) return false;
		return true;
	}

	bool IsMonsterMob(const AMCLiving* L)
	{
		const AMCMob* M = Cast<AMCMob>(L);
		return M && M->Def && M->Def->Category == EMCMobCategory::Monster && M->Def->Id != TEXT("creeper") && !M->bTamed;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Target selection shared by hostile / neutral mobs

void AMCMob::TickCommonTargeting()
{
	if (!Def) return;
	AMCLiving* T = Target.Get();
	if (T && !IsValidTarget(T, this)) { Target.Reset(); T = nullptr; }
	if (T)
	{
		// forget targets that are far away or long out of sight
		const float Range = Def->FollowRange;
		if (FVector::DistSquared(T->Pos, Pos) > Range * Range * 1.5f) { Target.Reset(); return; }
		if (!CanSee(T)) { if (++NoActionTime > 200 && !bAngry) Target.Reset(); }
		return;
	}
	if (Game && Game->Difficulty == EMCDifficulty::Peaceful && Def->Category == EMCMobCategory::Monster) return;
	if (!Def->bHostile && !bAngry) return;
	if (Age % 10 != 0) return;
	// spiders are passive in bright light, piglins ignore players wearing gold
	if (Def->AI == EMCMobAI::Spider && GetBrightness() > 0.5f && !bAngry) return;
	AMCPlayer* P = FindNearestPlayer(Def->FollowRange, true);
	if (P && Def->Id == TEXT("piglin"))
	{
		bool bGold = false;
		for (int32 s = 36; s < 40; ++s) { const FMCItemStack& A = P->Inventory.Slots[s]; if (!A.IsEmpty() && A.Item().ArmorMaterial == TEXT("golden")) bGold = true; }
		if (bGold && !bAngry) P = nullptr;
	}
	if (P) { Target = P; return; }
	// some mobs also target other mobs
	const FName Id = Def->Id;
	if (Id == TEXT("zombie") || Id == TEXT("husk") || Id == TEXT("drowned") || Id == TEXT("zombie_villager") || Id == TEXT("vindicator") || Id == TEXT("pillager") || Id == TEXT("evoker") || Id == TEXT("ravager"))
	{
		if (AMCLiving* V = FindNearestMob(16.f, [](AMCLiving* L) { const AMCMob* M = Cast<AMCMob>(L); return M && M->Def && (M->Def->Id == TEXT("villager") || M->Def->Id == TEXT("wandering_trader") || M->Def->Id == TEXT("iron_golem")); }))
			Target = V;
	}
	else if (Id == TEXT("wither_skeleton") || Id == TEXT("piglin_brute"))
	{
		// nothing extra
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Animals: panic, breed, tempt, follow parent, graze, wander

void AMCMob::TickAnimalAI()
{
	if (!Def) return;
	const FName Id = Def->Id;
	// neutral animals fight back
	if ((Def->bNeutral || Id == TEXT("goat") || Id == TEXT("panda") || Id == TEXT("fox") || Id == TEXT("llama") || Id == TEXT("trader_llama")) && Target.IsValid() && bAngry)
	{
		AMCLiving* T = Target.Get();
		if (IsValidTarget(T, this))
		{
			if (Id == TEXT("llama") || Id == TEXT("trader_llama"))
			{
				FaceTowards(T->Pos, 20.f);
				MoveForward = 0.f;
				if (--SpecialTimer <= 0 && CanSee(T)) { ShootProjectile(TEXT("llama_spit"), T, 1.5f, 1.f); SpecialTimer = 40; PlaySound(TEXT("llama_spit"), 1.f, 1.f); }
				return;
			}
			if (Id == TEXT("goat") && FVector::DistSquared(T->Pos, Pos) < 100.0 && SpecialTimer <= 0)
			{
				// ram
				const FVector D = (T->Pos - Pos).GetSafeNormal2D();
				Vel += D * 0.7;
				SpecialTimer = 100;
				PlaySound(TEXT("goat_ram_impact"), 1.f, 1.f);
			}
			if (SpecialTimer > 0) --SpecialTimer;
			NavigateTo(T->Pos, 1.25f);
			DoMeleeAttack(T);
			return;
		}
		Target.Reset();
	}
	// panic
	if (PanicTime > 0 || IsOnFire())
	{
		if (!bHasWander || Rand().NextInt(20) == 0)
		{
			FVector T;
			if (FindRandomWanderTarget(5, 4, T)) { WanderTarget = T; bHasWander = true; StopNavigation(); }
		}
		if (bHasWander) NavigateTo(WanderTarget, Id == TEXT("rabbit") ? 2.2f : 1.25f);
		return;
	}
	// breeding: find a partner in love
	if (InLove > 0 && World)
	{
		AMCLiving* Partner = FindNearestMob(8.f, [this](AMCLiving* L) { const AMCMob* M = Cast<AMCMob>(L); return M && M != this && M->Def == Def && M->InLove > 0 && M->GrowAge == 0; });
		if (AMCMob* PM = Cast<AMCMob>(Partner))
		{
			NavigateTo(PM->Pos, 1.f);
			FaceTowards(PM->Pos, 10.f);
			if (FVector::DistSquared(PM->Pos, Pos) < 4.0)
			{
				if (++SubState >= 60) { SubState = 0; Breed(PM); }
			}
			return;
		}
	}
	// tempted by a player holding food
	if (Game && Game->Player && Age % 5 == 0) FollowTarget = (IsTemptedBy(Game->Player) && FVector::DistSquared(Game->Player->Pos, Pos) < 100.0) ? Game->Player : nullptr;
	if (AMCEntity* F = FollowTarget.Get())
	{
		if (AMCPlayer* P = Cast<AMCPlayer>(F))
		{
			if (IsTemptedBy(P))
			{
				LookAt(P->GetEyePos(), 10.f, 10.f);
				if (FVector::DistSquared(P->Pos, Pos) > 6.25) NavigateTo(P->Pos, 1.2f);
				else { StopNavigation(); MoveForward = 0.f; }
				return;
			}
		}
	}
	// babies follow a parent
	if (IsBaby() && Age % 20 == 0)
	{
		AMCLiving* Parent = FindNearestMob(8.f, [this](AMCLiving* L) { const AMCMob* M = Cast<AMCMob>(L); return M && M->Def == Def && !M->IsBaby(); });
		FollowTarget = Parent;
	}
	if (IsBaby() && FollowTarget.IsValid() && FollowTarget->IsLiving())
	{
		const double D = FVector::DistSquared(FollowTarget->Pos, Pos);
		if (D > 9.0 && D < 256.0) { NavigateTo(FollowTarget->Pos, 1.1f); return; }
	}
	// grazing: sheep eat grass to regrow wool
	if (Id == TEXT("sheep") && World)
	{
		if (EatAnim > 0)
		{
			if (--EatAnim == 4)
			{
				const FMCBlockPos B = BlockBelow();
				const FName N = FMCBlocks::GetByState(World->GetState(B)).Name;
				const FMCBlockPos At = BlockPos();
				if (FMCBlocks::GetByState(World->GetState(At)).Name == TEXT("short_grass")) { World->DestroyBlock(At, false); bSheared = false; ApplyVariantVisuals(); if (IsBaby()) GrowAge = FMath::Min(0, GrowAge + 60); }
				else if (N == TEXT("grass_block")) { World->SetState(B, FMCBlocks::C.Dirt); World->PlaySound(TEXT("block_grass_break"), Pos, 1.f, 1.f); bSheared = false; ApplyVariantVisuals(); }
			}
			MoveForward = 0.f;
			return;
		}
		if (Rand().NextInt(IsBaby() ? 50 : 1000) == 0) { EatAnim = 40; StopNavigation(); return; }
	}
	// chickens lay eggs
	if (Id == TEXT("chicken") && !IsBaby() && --LayEggTime <= 0 && World)
	{
		World->SpawnItem(Pos, FMCItemStack::Of(TEXT("egg"), 1));
		PlaySound(TEXT("chicken_egg"), 1.f, (Rand().NextFloat() - Rand().NextFloat()) * 0.2f + 1.f);
		LayEggTime = 6000 + Rand().NextInt(6000);
	}
	if (Id == TEXT("chicken") && !bOnGround && Vel.Z < 0.0) Vel.Z *= 0.6; // flap
	// armadillos roll up near danger; rabbits hop
	if (Id == TEXT("rabbit") && bOnGround && MoveForward > 0.f && Rand().NextInt(10) == 0) Vel.Z = 0.35;
	// strider: prefers lava
	TickWander(1.f, Id == TEXT("strider") ? 60 : 120);
	// look at nearby players
	if (MoveForward == 0.f && Game && Game->Player && FVector::DistSquared(Game->Player->Pos, Pos) < 64.0 && Rand().NextInt(40) == 0) LookAt(Game->Player->GetEyePos(), 30.f, 20.f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Melee chasers

void AMCMob::TickZombieAI()
{
	if (!Def) return;
	const FName Id = Def->Id;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	// creaking: moves only while nobody looks at it
	if (Id == TEXT("creaking") && Game && Game->Player)
	{
		AMCPlayer* P = Game->Player;
		const FVector ToMe = (Pos + FVector(0, 0, Height * 0.5) - P->GetEyePos()).GetSafeNormal();
		const bool bWatched = FVector::DotProduct(P->GetLookDir(), ToMe) > 0.6 && P->CanSee(this);
		if (bWatched) { MoveForward = 0.f; Vel.X = Vel.Y = 0.0; return; }
		if (!T && FVector::DistSquared(P->Pos, Pos) < 24.0 * 24.0 && !P->IsCreative()) Target = T = P;
	}
	// burning undead seek shade / water (simplified: run to a darker spot)
	if (!T)
	{
		if (Def->bBurnsInDaylight && IsOnFire() && Rand().NextInt(20) == 0)
		{
			FVector Shade;
			if (FindRandomWanderTarget(10, 3, Shade)) { WanderTarget = Shade; bHasWander = true; }
		}
		TickWander(1.f, 120);
		return;
	}
	// piglins admire gold instead of fighting
	if (Id == TEXT("piglin") && Anger > 0)
	{
		--Anger;
		MoveForward = 0.f;
		if (Anger == 0 && World)
		{
			GetItem(EMCEquipSlot::OffHand).Clear();
			World->SpawnItem(Pos + FVector(0, 0, 1), MCLoot::Barter(Rand()));
		}
		return;
	}
	// ranged piglins with crossbows
	if (Id == TEXT("piglin") && MainHandConst().Item().Kind == EMCItemKind::Crossbow)
	{
		TickSkeletonAI();
		return;
	}
	// drowned with tridents throw them
	if (Id == TEXT("drowned") && MainHandConst().Item().Kind == EMCItemKind::Trident && FVector::DistSquared(T->Pos, Pos) > 16.0 && CanSee(T))
	{
		FaceTowards(T->Pos, 30.f);
		if (--ShootTime <= 0) { ShootProjectile(TEXT("trident"), T, 1.6f, 1.f); ShootTime = 40; PlaySound(TEXT("drowned_shoot"), 1.f, 1.f); }
		MoveForward = 0.f;
		return;
	}
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const double D = FVector::DistSquared(T->Pos, Pos);
	if (D > AttackReach(T))
	{
		NavigateTo(T->Pos, Id == TEXT("silverfish") || Id == TEXT("endermite") ? 1.0f : (Def->AI == EMCMobAI::Zombie ? 1.0f : 1.2f));
	}
	else
	{
		MoveTowards(T->Pos, 1.f);
	}
	DoMeleeAttack(T);
	// zombies break wooden doors on hard difficulty
	if ((Id == TEXT("zombie") || Id == TEXT("husk") || Id == TEXT("zombie_villager")) && bHorizontalCollision && Game && Game->Difficulty == EMCDifficulty::Hard && World)
	{
		const FMCBlockPos Front = FMCBlockPos(MC::FloorToInt(Pos.X + FMath::Cos(FMath::DegreesToRadians(Yaw)) * 0.8), MC::FloorToInt(Pos.Y + FMath::Sin(FMath::DegreesToRadians(Yaw)) * 0.8), MC::FloorToInt(Pos.Z));
		const FMCBlock& B = FMCBlocks::GetByState(World->GetState(Front));
		if (B.Model == EMCModel::Door && B.Name != TEXT("iron_door"))
		{
			if (++SubState % 20 == 0) World->PlaySound(TEXT("zombie_attack_wooden_door"), FVector(Front.X + 0.5, Front.Y + 0.5, Front.Z + 0.5), 1.f, 1.f);
			if (SubState >= 240) { World->DestroyBlock(Front, true); SubState = 0; }
		}
	}
	// silverfish call friends out of infested blocks
	if (Id == TEXT("silverfish") && HurtTime == 9 && World)
	{
		for (int32 dx = -5; dx <= 5; ++dx) for (int32 dy = -5; dy <= 5; ++dy) for (int32 dz = -2; dz <= 2; ++dz)
		{
			const FMCBlockPos P = BlockPos() + FIntVector(dx, dy, dz);
			const FString N = FMCBlocks::GetByState(World->GetState(P)).Name.ToString();
			if (N.StartsWith(TEXT("infested_")) && Rand().NextInt(3) == 0)
			{
				World->DestroyBlock(P, false);
				if (Game) Game->SpawnMob(World, TEXT("silverfish"), FVector(P.X + 0.5, P.Y + 0.5, P.Z), false);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Ranged: skeletons, strays, bogged, parched, pillagers, piglin crossbowmen

void AMCMob::TickSkeletonAI()
{
	if (!Def) return;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	if (!T)
	{
		if (Def->bBurnsInDaylight && IsOnFire() && Rand().NextInt(20) == 0) { FVector S; if (FindRandomWanderTarget(10, 3, S)) { WanderTarget = S; bHasWander = true; } }
		TickWander(1.f, 120);
		ChargeTime = 0;
		return;
	}
	const double D = FVector::Dist(T->Pos, Pos);
	const bool bSee = CanSee(T);
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const bool bCrossbow = MainHandConst().Item().Kind == EMCItemKind::Crossbow;
	if (!bCrossbow && MainHandConst().Item().Kind != EMCItemKind::Bow && Def->Id != TEXT("pillager"))
	{
		// lost its bow: melee
		NavigateTo(T->Pos, 1.f);
		DoMeleeAttack(T);
		return;
	}
	// keep ~12 blocks, strafe when close
	if (D > 15.0 || !bSee) NavigateTo(T->Pos, 1.f);
	else
	{
		StopNavigation();
		if (D < 6.0) { MoveForward = -0.5f; SpeedModifier = 1.f; }
		else MoveForward = 0.f;
		if (++SubState % 40 == 0) SwellDir = Rand().NextBool() ? 1 : -1;
		MoveStrafe = 0.5f * SwellDir;
	}
	if (!bSee) { ChargeTime = FMath::Max(0, ChargeTime - 1); return; }
	++ChargeTime;
	const int32 Draw = bCrossbow ? 25 : 20;
	const int32 Interval = Game && Game->Difficulty == EMCDifficulty::Hard ? 20 : 40;
	if (ChargeTime >= Draw && --ShootTime <= 0)
	{
		const FName Id = Def->Id;
		FName Arrow = TEXT("arrow");
		if (Id == TEXT("stray")) Arrow = TEXT("slow_arrow");
		else if (Id == TEXT("bogged")) Arrow = TEXT("poison_arrow");
		else if (Id == TEXT("parched")) Arrow = TEXT("weakness_arrow");
		ShootProjectile(Arrow, T, bCrossbow ? 3.15f : 1.6f, 1.f);
		PlaySound(bCrossbow ? TEXT("crossbow_shoot") : TEXT("skeleton_shoot"), 1.f, 1.f / (Rand().NextFloat() * 0.4f + 0.8f));
		ShootTime = Interval;
		ChargeTime = 0;
		Swing();
	}
	MoveStrafe = FMath::Clamp(MoveStrafe, -1.f, 1.f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Creeper

void AMCMob::TickCreeperAI()
{
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	// flee from cats and ocelots
	if (Age % 20 == 0)
	{
		if (AMCLiving* Cat = FindNearestMob(6.f, [](AMCLiving* L) { const AMCMob* M = Cast<AMCMob>(L); return M && M->Def && (M->Def->Id == TEXT("cat") || M->Def->Id == TEXT("ocelot")); }))
		{
			const FVector Away = Pos + (Pos - Cat->Pos).GetSafeNormal() * 8.0;
			WanderTarget = Away; bHasWander = true; PanicTime = 40;
		}
	}
	if (PanicTime > 0 && bHasWander) { NavigateTo(WanderTarget, 1.2f); SwellDir = -1; }
	else if (T)
	{
		const double D = FVector::DistSquared(T->Pos, Pos);
		LookAt(T->GetEyePos(), 30.f, 30.f);
		if (D < 9.0 && CanSee(T)) { SwellDir = 1; StopNavigation(); MoveForward = 0.f; }
		else
		{
			if (D > 49.0) SwellDir = -1;
			NavigateTo(T->Pos, 1.f);
		}
	}
	else
	{
		SwellDir = -1;
		TickWander(0.8f, 120);
	}
	if (SwellDir > 0 && SwellTime == 0) PlaySound(TEXT("creeper_primed"), 1.f, 0.5f);
	SwellTime = FMath::Clamp(SwellTime + SwellDir, 0, 30);
	if (SwellTime >= 30) Explode();
}

// ---------------------------------------------------------------------------------------------------------------------
// Spiders: pounce and climb walls

void AMCMob::TickSpiderAI()
{
	// daylight makes spiders passive unless provoked
	if (Target.IsValid() && !bAngry && GetBrightness() > 0.5f && Rand().NextInt(100) == 0) Target.Reset();
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	if (bHorizontalCollision) { Vel.Z = FMath::Max(Vel.Z, 0.2); FallDistance = 0.f; }
	if (!T) { TickWander(1.f, 120); return; }
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const double D = FVector::DistSquared(T->Pos, Pos);
	if (D > 4.0 && D < 16.0 && bOnGround && Rand().NextInt(10) == 0)
	{
		// leap
		const FVector Dir = (T->Pos - Pos).GetSafeNormal2D();
		Vel.X += Dir.X * 0.4; Vel.Y += Dir.Y * 0.4; Vel.Z = 0.4;
	}
	NavigateTo(T->Pos, 1.f);
	DoMeleeAttack(T);
}

// ---------------------------------------------------------------------------------------------------------------------
// Enderman: stare aggro, teleporting, block carrying

void AMCMob::TickEndermanAI()
{
	if (!World || !Def) return;
	AMCPlayer* P = Game ? Game->Player : nullptr;
	// looking at an enderman's head provokes it (carved pumpkin prevents)
	if (P && !bAngry && P->World == World && !P->IsCreative() && !P->IsSpectator() && FVector::DistSquared(P->Pos, Pos) < 64.0 * 64.0)
	{
		const FMCItemStack& Head = P->Inventory.Slots[39];
		const bool bPumpkin = !Head.IsEmpty() && Head.Item().Name == TEXT("carved_pumpkin");
		if (!bPumpkin)
		{
			const FVector ToHead = (GetEyePos() - P->GetEyePos());
			const double Dist = ToHead.Size();
			const double Dot = FVector::DotProduct(P->GetLookDir(), ToHead / FMath::Max(Dist, 0.01));
			if (Dot > 1.0 - 0.025 / FMath::Max(Dist, 1.0) && P->CanSee(this)) { SetAngryAt(P, 600); SpecialTimer = 5; }
		}
	}
	AMCLiving* T = Target.Get();
	if (T && !IsValidTarget(T, this)) { Target.Reset(); T = nullptr; bAngry = false; }
	if (T)
	{
		const double D = FVector::DistSquared(T->Pos, Pos);
		LookAt(T->GetEyePos(), 30.f, 30.f);
		if (D > 256.0 && Rand().NextInt(30) == 0) TeleportTowards(T->Pos + FVector(Rand().FRange(-4.f, 4.f), Rand().FRange(-4.f, 4.f), 0));
		NavigateTo(T->Pos, 1.f);
		DoMeleeAttack(T);
		return;
	}
	// idle: wander, randomly teleport, pick up / place blocks
	if (Rand().NextInt(600) == 0) TeleportRandomly(32.f);
	if (Game && Game->Rules.bMobGriefing)
	{
		if (CarriedBlock == 0 && Rand().NextInt(20) == 0)
		{
			const FMCBlockPos B = BlockPos() + FIntVector(Rand().Range(-2, 2), Rand().Range(-2, 2), Rand().Range(0, 3));
			const FMCState S = World->GetState(B);
			const FMCBlock& Blk = FMCBlocks::GetByState(S);
			if (Blk.HasTag(TEXT("enderman_holdable")) || Blk.Name == TEXT("grass_block") || Blk.Name == TEXT("dirt") || Blk.Name == TEXT("sand") || Blk.Name == TEXT("gravel") || Blk.Name == TEXT("pumpkin") || Blk.Name == TEXT("melon") || Blk.Name == TEXT("clay") || Blk.Has(MCB_Plant) && Blk.Shape == EMCShape::Cross)
			{
				CarriedBlock = S;
				World->SetState(B, 0);
			}
		}
		else if (CarriedBlock != 0 && Rand().NextInt(2000) == 0)
		{
			const FMCBlockPos B = BlockPos() + FIntVector(Rand().Range(-1, 1), Rand().Range(-1, 1), Rand().Range(0, 2));
			if (World->GetState(B) == 0 && FMCBlocks::IsSolid(World->GetState(B.Down())) && FMCBlocks::IsOpaque(World->GetState(B.Down())))
			{
				World->SetState(B, CarriedBlock);
				CarriedBlock = 0;
			}
		}
	}
	TickWander(1.f, 120);
}

// ---------------------------------------------------------------------------------------------------------------------
// Slimes, magma cubes, sulfur cubes: hop towards the target

void AMCMob::TickSlimeAI()
{
	if (!Def) return;
	const FName Id = Def->Id;
	if (Id != TEXT("sulfur_cube")) TickCommonTargeting();
	AMCLiving* T = Target.Get();
	MoveForward = 0.f;
	if (T) FaceTowards(T->Pos, 10.f);
	else if (Rand().NextInt(40) == 0) Yaw += Rand().FRange(-90.f, 90.f);
	if (bOnGround)
	{
		if (--ShootTime <= 0)
		{
			ShootTime = (T ? 10 : 20) + Rand().NextInt(20);
			if (T) ShootTime /= 3;
			const double R = FMath::DegreesToRadians((double)Yaw);
			const double F = (0.1 + 0.04 * SplitSize) * (T || Rand().NextInt(3) == 0 ? 1.0 : 0.0);
			Vel.X += FMath::Cos(R) * F;
			Vel.Y += FMath::Sin(R) * F;
			Vel.Z = Id == TEXT("magma_cube") ? 0.42 + 0.1 * SplitSize : 0.42;
			if (F > 0.0) PlaySound(FName(*FString::Printf(TEXT("%s_jump"), *Def->Sound.ToString())), 0.3f * SplitSize, 1.f);
		}
		else { Vel.X *= 0.5; Vel.Y *= 0.5; }
	}
	else
	{
		// keep momentum in the air
		const double R = FMath::DegreesToRadians((double)Yaw);
		if (T) { Vel.X += FMath::Cos(R) * 0.005 * SplitSize; Vel.Y += FMath::Sin(R) * 0.005 * SplitSize; }
	}
	// contact damage (not the smallest size)
	if (T && SplitSize > 1 && Id != TEXT("sulfur_cube") && FVector::DistSquared(T->Pos, Pos) < FMath::Square(0.6 * SplitSize + 0.4) && CanSee(T) && AttackCooldown <= 0)
	{
		AttackCooldown = 10;
		FMCDamage D = FMCDamage::Mob(this);
		T->Hurt(D, Id == TEXT("magma_cube") ? SplitSize * 2.f : (float)SplitSize);
		PlaySound(TEXT("slime_attack"), 1.f, 1.f);
	}
	// sulfur cubes release a stinging cloud when provoked
	if (Id == TEXT("sulfur_cube") && bAngry && T && FVector::DistSquared(T->Pos, Pos) < 9.0 && SpecialTimer <= 0 && Game && World)
	{
		if (AMCAreaCloud* C = Game->SpawnEntity<AMCAreaCloud>(World, Pos))
		{
			C->Effect = EMCEffect::Poison; C->EffectDuration = 100; C->Radius = 2.5f; C->Duration = 100; C->Color = FColor(220, 200, 60); C->Owner = this;
		}
		World->SpawnParticles(TEXT("sulfur_spark"), Pos + FVector(0, 0, Height * 0.5), 30, 1.2f, FVector(0, 0, 0.1), FColor(240, 220, 80));
		SpecialTimer = 120;
	}
	if (SpecialTimer > 0) --SpecialTimer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ghast: float around, shoot fireballs

void AMCMob::TickGhastAI()
{
	if (!World) return;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	if (FVector::DistSquared(FlyTarget, Pos) < 1.0 || FVector::DistSquared(FlyTarget, Pos) > 3600.0 || Rand().NextInt(100) == 0)
	{
		FlyTarget = Pos + FVector(Rand().FRange(-16.f, 16.f), Rand().FRange(-16.f, 16.f), Rand().FRange(-16.f, 16.f));
		FlyTarget.Z = FMath::Clamp(FlyTarget.Z, 8.0, 120.0);
	}
	TickFlyMovement(0.08f);
	if (T && FVector::DistSquared(T->Pos, Pos) < 64.0 * 64.0 && CanSee(T))
	{
		LookAt(T->GetEyePos(), 10.f, 10.f);
		++ChargeTime;
		if (ChargeTime == 10) PlaySound(TEXT("ghast_warn"), 10.f, 1.f);
		if (ChargeTime >= 20)
		{
			ShootProjectile(TEXT("fireball"), T, 1.2f, 0.f);
			PlaySound(TEXT("ghast_shoot"), 10.f, 1.f);
			ChargeTime = -40;
		}
	}
	else if (ChargeTime > 0) --ChargeTime;
}

// ---------------------------------------------------------------------------------------------------------------------
// Blaze & breeze: hover, burst fire / wind charges

void AMCMob::TickBlazeAI()
{
	if (!World || !Def) return;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	const bool bBreeze = Def->Id == TEXT("breeze");
	if (!bOnGround && Vel.Z < 0.0) Vel.Z *= 0.6;
	if (World->Rand.NextInt(bBreeze ? 40 : 200) == 0 && !bBreeze) PlaySound(TEXT("blaze_burn"), 1.f, 1.f);
	if (!T) { TickWander(1.f, 80); return; }
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const double D = FVector::DistSquared(T->Pos, Pos);
	// hover slightly above the target
	if (!bBreeze && T->GetEyePos().Z > GetEyePos().Z && Rand().NextInt(10) == 0) Vel.Z += 0.3;
	if (bBreeze && bOnGround && Rand().NextInt(20) == 0)
	{
		// breezes hop around their target
		const FVector Side = FVector(-(T->Pos - Pos).Y, (T->Pos - Pos).X, 0).GetSafeNormal() * (Rand().NextBool() ? 1.0 : -1.0);
		Vel += Side * 0.5 + FVector(0, 0, 0.7);
		PlaySound(TEXT("breeze_jump"), 1.f, 1.f);
	}
	if (D < 4.0 && !bBreeze) { MoveTowards(T->Pos, 1.f); DoMeleeAttack(T); }
	else if (D > 64.0) NavigateTo(T->Pos, 1.f);
	else { StopNavigation(); MoveForward = 0.f; }
	if (!CanSee(T)) return;
	++ChargeTime;
	if (bBreeze)
	{
		if (ChargeTime >= 40) { ShootProjectile(TEXT("wind_charge"), T, 0.9f, 0.5f); PlaySound(TEXT("breeze_shoot"), 1.f, 1.f); ChargeTime = 0; }
		return;
	}
	// blaze: 60 tick cycle -> 3 fireballs 6 ticks apart after charging
	if (ChargeTime == 60) World->SpawnParticles(TEXT("flame"), Pos + FVector(0, 0, Height * 0.5), 20, 0.5f, FVector(0, 0, 0.05));
	if (ChargeTime > 60 && (ChargeTime - 60) % 6 == 0 && ChargeTime <= 78)
	{
		ShootProjectile(TEXT("small_fireball"), T, 1.f, 0.5f);
		PlaySound(TEXT("blaze_shoot"), 1.f, 1.f);
	}
	if (ChargeTime > 100) ChargeTime = 0;
}
