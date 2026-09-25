// Player interaction: targeting, mining, melee combat, block placement and every item's right-click behaviour.
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Blocks/MCBehaviorsInternal.h"
#include "Items/MCLoot.h"
#include "Gen/MCWorldGen.h"
#include "Serialization/MemoryReader.h"

namespace
{
	bool IsMergeModel(const FMCBlock& B)
	{
		return B.Model == EMCModel::Slab || B.Model == EMCModel::SnowLayer || B.Model == EMCModel::Candle || B.Model == EMCModel::SeaPickle
			|| B.Model == EMCModel::TurtleEgg || B.Name == TEXT("pink_petals") || B.Name == TEXT("wildflowers") || B.Name == TEXT("leaf_litter");
	}

	FName StrippedOf(FName N)
	{
		const FString S = N.ToString();
		if (S.StartsWith(TEXT("stripped_"))) return NAME_None;
		if (S.EndsWith(TEXT("_log")) || S.EndsWith(TEXT("_wood")) || S.EndsWith(TEXT("_stem")) || S.EndsWith(TEXT("_hyphae")) || S == TEXT("bamboo_block"))
		{
			const FName R(*(TEXT("stripped_") + S));
			return FMCBlocks::Find(R) ? R : NAME_None;
		}
		return NAME_None;
	}

	/** Axe on copper: remove wax, then scrape one oxidation stage. */
	FName ScrapedOf(FName N)
	{
		FString S = N.ToString();
		if (S.StartsWith(TEXT("waxed_"))) { const FName R(*S.Mid(6)); return FMCBlocks::Find(R) ? R : NAME_None; }
		static const TCHAR* Stages[] = { TEXT("oxidized_"), TEXT("weathered_"), TEXT("exposed_") };
		static const TCHAR* Prev[] = { TEXT("weathered_"), TEXT("exposed_"), TEXT("") };
		for (int32 i = 0; i < 3; ++i)
		{
			if (!S.StartsWith(Stages[i])) continue;
			FString Base = S.Mid(FCString::Strlen(Stages[i]));
			FString R = FString(Prev[i]) + Base;
			if (R == TEXT("copper")) R = TEXT("copper_block");
			return FMCBlocks::Find(FName(*R)) ? FName(*R) : NAME_None;
		}
		return NAME_None;
	}

	float BowPower(int32 UseTicks)
	{
		float F = UseTicks / 20.f;
		F = (F * F + F * 2.f) / 3.f;
		return FMath::Min(F, 1.f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Targeting

float AMCPlayer::GetReach() const
{
	return IsCreative() ? 5.f : 4.5f;
}

bool AMCPlayer::GetTarget(FMCRayHit& OutBlock, AMCEntity*& OutEntity) const
{
	OutEntity = nullptr;
	OutBlock = FMCRayHit();
	if (!World) return false;
	const FVector Eye = GetEyePos();
	const FVector Dir = FRotator(ViewPitch, ViewYaw, 0.f).Vector();
	const double Reach = GetReach();
	const bool bHitBlock = World->Raycast(Eye, Dir, Reach, OutBlock, false, true);
	const double BlockDist = bHitBlock ? OutBlock.Distance : Reach;

	// entities: survival 3 blocks, creative 5 (plus weapon reach bonus)
	double EntityReach = IsCreative() ? 5.0 : 3.0;
	if (!HeldConst().IsEmpty()) EntityReach += HeldConst().Item().Reach;
	const double MaxE = FMath::Min(BlockDist, EntityReach);
	TArray<AMCEntity*> Near;
	const FVector End = Eye + Dir * MaxE;
	FMCBox Sweep(FVector(FMath::Min(Eye.X, End.X), FMath::Min(Eye.Y, End.Y), FMath::Min(Eye.Z, End.Z)), FVector(FMath::Max(Eye.X, End.X), FMath::Max(Eye.Y, End.Y), FMath::Max(Eye.Z, End.Z)));
	World->GetEntitiesInBox(Sweep.Inflate(3.0), Near, this);
	double Best = MaxE + 1e-3;
	for (AMCEntity* E : Near)
	{
		if (!E->IsPickable() || E->bRemoved || E == Vehicle.Get()) continue;
		double T = 0.0;
		EMCFace F;
		if (const AMCEnderDragon* Dr = Cast<AMCEnderDragon>(E))
		{
			if (Dr->RayHitParts(Eye, Dir, MaxE, T) && T < Best) { Best = T; OutEntity = E; }
			continue;
		}
		const FMCBox B = E->GetBox().Inflate(0.1);
		if (B.Contains(Eye)) { Best = 0.0; OutEntity = E; continue; }
		if (B.RayHit(Eye, Dir, MaxE, T, F) && T < Best) { Best = T; OutEntity = E; }
	}
	if (OutEntity) { OutBlock.bHit = false; return true; }
	return bHitBlock;
}

// ---------------------------------------------------------------------------------------------------------------------
// Mining

float AMCPlayer::GetDestroySpeed(FMCState S) const
{
	const FMCBlock& B = FMCBlocks::GetByState(S);
	const FMCItemStack& H = HeldConst();
	float Speed = 1.f;
	if (!H.IsEmpty())
	{
		const FMCItem& I = H.Item();
		const bool bSwordWeb = I.ToolType == EMCTool::Sword && (B.Name == TEXT("cobweb"));
		const bool bShears = I.ToolType == EMCTool::Shears && (B.Has(MCB_Leaves) || B.Has(MCB_Wool) || B.Name == TEXT("cobweb") || B.Name == TEXT("vine") || B.Name == TEXT("glow_lichen"));
		if (bSwordWeb) Speed = 15.f;
		else if (bShears) Speed = B.Name == TEXT("cobweb") ? 15.f : (B.Has(MCB_Wool) ? 5.f : 15.f);
		else if (I.ToolType != EMCTool::None && I.ToolType == B.Tool) Speed = I.MiningSpeed;
		else if (I.ToolType == EMCTool::Sword && (B.Has(MCB_Leaves) || B.Has(MCB_Plant) || B.Name == TEXT("bamboo") || B.Name == TEXT("melon") || B.Name == TEXT("pumpkin"))) Speed = 1.5f;
		if (Speed > 1.f)
		{
			const int32 Eff = H.GetEnchant(EMCEnchant::Efficiency);
			if (Eff > 0) Speed += Eff * Eff + 1;
		}
	}
	const int32 Haste = EffectAmp(EMCEffect::Haste);
	if (Haste >= 0) Speed *= 1.f + 0.2f * (Haste + 1);
	const int32 Fatigue = EffectAmp(EMCEffect::MiningFatigue);
	if (Fatigue >= 0)
	{
		static const float F[4] = { 0.3f, 0.09f, 0.0027f, 0.00081f };
		Speed *= F[FMath::Min(Fatigue, 3)];
	}
	if (bEyesInWater && GetEnchantMax(EMCEnchant::AquaAffinity) == 0) Speed /= 5.f;
	if (!bOnGround && !bFlying) Speed /= 5.f;
	return Speed;
}

bool AMCPlayer::CanHarvest(FMCState S) const
{
	const FMCItemStack& H = HeldConst();
	return MCLoot::CanHarvest(S, H.IsEmpty() ? nullptr : &H);
}

void AMCPlayer::StopMining()
{
	bMining = false;
	MiningProgress = 0.f;
	MiningTicks = 0;
}

void AMCPlayer::TickMining()
{
	if (!World) return;
	const bool bPressed = Input.bAttackPressed;
	const bool bHeld = Input.bAttackHeld;
	Input.bAttackPressed = false;
	if (IsMenuOpen() || Health <= 0.f || IsSpectator() || bSleeping) { StopMining(); return; }
	HandleAttack(bPressed, bHeld);
}

void AMCPlayer::HandleAttack(bool bPressed, bool bHeld)
{
	if (!bPressed && !bHeld) { StopMining(); return; }
	FMCRayHit Hit;
	AMCEntity* Target = nullptr;
	GetTarget(Hit, Target);

	if (bPressed)
	{
		Swing();
		if (Target)
		{
			AttackEntity(Target);
			StopMining();
			return;
		}
		if (!Hit.bHit)
		{
			AttackStrengthTicker = 0;
			StopMining();
			return;
		}
	}
	if (!Hit.bHit || Target) { StopMining(); return; }
	if (GameMode == EMCGameMode::Adventure) return;

	const FMCState S = Hit.State;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	if (IsCreative())
	{
		// swords, tridents and maces cannot break blocks in creative
		const FMCItemStack& H = HeldConst();
		if (!H.IsEmpty() && (H.Item().ToolType == EMCTool::Sword || H.Item().Kind == EMCItemKind::Trident || H.Item().Kind == EMCItemKind::Mace || H.Item().Kind == EMCItemKind::Spear)) return;
		if (bPressed || DestroyDelay == 0)
		{
			if (bPressed) B.Behavior->OnAttack(*World, Hit.Pos, S, this);
			if (World->GetState(Hit.Pos) == S)
			{
				World->DestroyBlock(Hit.Pos, false, this, nullptr, true);
				++StatBlocksMined;
			}
			DestroyDelay = 5;
		}
		return;
	}

	if (!bMining || !(MiningPos == Hit.Pos))
	{
		if (bMining && DestroyDelay > 0) return;
		bMining = true;
		MiningPos = Hit.Pos;
		MiningProgress = 0.f;
		MiningTicks = 0;
		B.Behavior->OnAttack(*World, Hit.Pos, S, this);
		if (World->GetState(Hit.Pos) != S) { StopMining(); return; }
	}
	if (DestroyDelay > 0) return;
	if (B.Hardness < 0.f || B.Has(MCB_Unbreakable)) return;
	const float Speed = GetDestroySpeed(S);
	const float Div = CanHarvest(S) ? 30.f : 100.f;
	const float Delta = B.Hardness <= 0.f ? 1.f : Speed / B.Hardness / Div;
	MiningProgress += Delta;
	++MiningTicks;
	if (MiningTicks % 4 == 1) World->PlayBlockSound(S, 3, FVector(Hit.Pos.X + 0.5, Hit.Pos.Y + 0.5, Hit.Pos.Z + 0.5));
	if (MiningTicks % 2 == 0 && Game) Game->SpawnBlockParticles(World, Hit.Pos, S, false);
	if (MiningProgress >= 1.f)
	{
		FMCItemStack Tool = HeldConst().Copy();
		const bool bHarvest = CanHarvest(S);
		World->DestroyBlock(Hit.Pos, bHarvest, this, Tool.IsEmpty() ? nullptr : &Tool, true);
		++StatBlocksMined;
		CauseExhaustion(0.005f);
		// tools lose durability when breaking non-instant blocks
		if (!HeldConst().IsEmpty() && HeldConst().IsDamageable() && B.Hardness > 0.f)
		{
			const bool bWeapon = HeldConst().Item().ToolType == EMCTool::Sword || HeldConst().Item().Kind == EMCItemKind::Trident;
			DamageHeld(bWeapon ? 2 : 1);
		}
		StopMining();
		DestroyDelay = 5;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Combat

float AMCPlayer::GetAttackDelayTicks() const
{
	float Speed = 4.f;
	if (!HeldConst().IsEmpty() && HeldConst().Item().AttackSpeed > 0.f) Speed = HeldConst().Item().AttackSpeed;
	const int32 Haste = EffectAmp(EMCEffect::Haste);
	if (Haste >= 0) Speed *= 1.f + 0.1f * (Haste + 1);
	const int32 Fatigue = EffectAmp(EMCEffect::MiningFatigue);
	if (Fatigue >= 0) Speed *= FMath::Max(0.1f, 1.f - 0.1f * (Fatigue + 1));
	return 20.f / FMath::Max(0.1f, Speed);
}

float AMCPlayer::GetAttackStrengthScale(float Adjust) const
{
	return FMath::Clamp((AttackStrengthTicker + Adjust) / GetAttackDelayTicks(), 0.f, 1.f);
}

bool AMCPlayer::AttackEntity(AMCEntity* Target)
{
	if (!Target || Target == this || !World) return false;
	if (IsSpectator()) return false;
	const FMCItemStack& H = HeldConst();
	const FMCItem* I = H.IsEmpty() ? nullptr : &H.Item();
	float Damage = I && I->AttackDamage > 0.f ? I->AttackDamage : 1.f;
	const int32 Str = EffectAmp(EMCEffect::Strength);
	if (Str >= 0) Damage += 3.f * (Str + 1);
	const int32 Weak = EffectAmp(EMCEffect::Weakness);
	if (Weak >= 0) Damage -= 4.f * (Weak + 1);
	float Bonus = 0.f;
	AMCLiving* LT = Cast<AMCLiving>(Target);
	if (!H.IsEmpty())
	{
		const int32 Sharp = H.GetEnchant(EMCEnchant::Sharpness);
		if (Sharp > 0) Bonus += 0.5f * Sharp + 0.5f;
		if (LT && LT->bUndead) Bonus += 2.5f * H.GetEnchant(EMCEnchant::Smite);
		if (LT && LT->bArthropod) Bonus += 2.5f * H.GetEnchant(EMCEnchant::BaneOfArthropods);
	}
	const float Strength = GetAttackStrengthScale(0.5f);
	Damage *= 0.2f + Strength * Strength * 0.8f;
	Bonus *= Strength;
	AttackStrengthTicker = 0;
	const bool bStrong = Strength > 0.9f;
	int32 KB = !H.IsEmpty() ? H.GetEnchant(EMCEnchant::Knockback) : 0;
	if (bSprinting && bStrong) { ++KB; PlaySound(TEXT("player_attack_knockback"), 1.f, 1.f); }
	const bool bCrit = bStrong && FallDistance > 0.f && !bOnGround && !OnClimbable() && !bInWater && !HasEffect(EMCEffect::Blindness) && !IsPassenger() && LT && !bSprinting;
	if (bCrit) Damage *= 1.5f;
	// mace smash: bonus from fall distance
	if (I && I->Kind == EMCItemKind::Mace && FallDistance > 1.5f)
	{
		const float F = FallDistance;
		float Smash = F <= 3.f ? 4.f * F : (F <= 8.f ? 12.f + 2.f * (F - 3.f) : 22.f + (F - 8.f));
		Smash += H.GetEnchant(EMCEnchant::Density) * 0.5f * F;
		Damage += Smash;
		FallDistance = 0.f;
		Vel.Z = FMath::Max(Vel.Z, 0.01);
		World->PlaySound(F > 5.f ? TEXT("mace_smash_ground_heavy") : TEXT("mace_smash_ground"), Target->Pos, 1.f, 1.f);
		// wind burst
		const int32 WB = H.GetEnchant(EMCEnchant::WindBurst);
		if (WB > 0) Vel.Z = 0.6 + 0.35 * WB;
		// area knockback
		TArray<AMCEntity*> Around;
		World->GetEntitiesInBox(Target->GetBox().Inflate(3.5), Around, this);
		for (AMCEntity* O : Around) if (AMCLiving* L = Cast<AMCLiving>(O)) if (O != Target) L->Knockback(0.7, Pos.X - O->Pos.X, Pos.Y - O->Pos.Y);
	}
	const float Total = Damage + Bonus;
	FMCDamage D = FMCDamage::PlayerAttack(this);
	D.bCritical = bCrit;
	const float HealthBefore = LT ? LT->Health : 0.f;
	const bool bHit = Target->Hurt(D, Total);
	if (bHit)
	{
		if (KB > 0 && LT)
		{
			const double R = FMath::DegreesToRadians((double)Yaw);
			LT->Knockback(KB * 0.5, -FMath::Cos(R), -FMath::Sin(R));
			Vel.X *= 0.6; Vel.Y *= 0.6;
			bSprinting = false;
		}
		// sweeping edge
		const bool bSweep = bStrong && !bCrit && !bSprinting && bOnGround && I && I->ToolType == EMCTool::Sword && FVector2D(Pos.X - PrevPos.X, Pos.Y - PrevPos.Y).Size() < GetSpeed() * 2.5f;
		if (bSweep)
		{
			const int32 SE = H.GetEnchant(EMCEnchant::SweepingEdge);
			const float SweepDmg = 1.f + (SE > 0 ? Total * (SE / (SE + 1.f)) : 0.f);
			TArray<AMCEntity*> Around;
			World->GetEntitiesInBox(Target->GetBox().Inflate(1.0).Expand(FVector(0, 0, 0.25)), Around, this);
			for (AMCEntity* O : Around)
			{
				AMCLiving* L = Cast<AMCLiving>(O);
				if (!L || O == Target || O->DistanceTo(this) > 3.0f || L->IsA<AMCPlayer>()) continue;
				const double R = FMath::DegreesToRadians((double)Yaw);
				L->Knockback(0.4, -FMath::Cos(R), -FMath::Sin(R));
				L->Hurt(FMCDamage::PlayerAttack(this), SweepDmg);
			}
			World->PlaySound(TEXT("player_attack_sweep"), Pos, 1.f, 1.f);
			World->SpawnParticles(TEXT("sweep_attack"), Target->Pos + FVector(0, 0, Target->Height * 0.5), 1, 0.f);
		}
		if (bCrit)
		{
			World->PlaySound(TEXT("player_attack_crit"), Pos, 1.f, 1.f);
			World->SpawnParticles(TEXT("crit"), Target->Pos + FVector(0, 0, Target->Height * 0.5), 12, Target->Width * 0.6f, FVector(0, 0, 0.2));
		}
		else if (!bSweep)
		{
			World->PlaySound(bStrong ? TEXT("player_attack_strong") : TEXT("player_attack_weak"), Pos, 1.f, 1.f);
		}
		if (Bonus > 0.f) World->SpawnParticles(TEXT("enchanted_hit"), Target->Pos + FVector(0, 0, Target->Height * 0.5), 10, Target->Width * 0.6f, FVector(0, 0, 0.2));
		const int32 FA = H.IsEmpty() ? 0 : H.GetEnchant(EMCEnchant::FireAspect);
		if (FA > 0) Target->SetOnFire(4 * FA);
		if (LT)
		{
			const float Dealt = HealthBefore - LT->Health;
			if (Dealt > 2.f) World->SpawnParticles(TEXT("damage_indicator"), Target->Pos + FVector(0, 0, Target->Height * 0.5), FMath::Min(10, (int32)(Dealt * 0.5f)), 0.3f, FVector(0, 0, 0.2));
		}
		if (!H.IsEmpty() && LT)
		{
			const bool bWeapon = I->ToolType == EMCTool::Sword || I->Kind == EMCItemKind::Trident || I->Kind == EMCItemKind::Mace || I->Kind == EMCItemKind::Spear;
			DamageHeld(bWeapon ? 1 : 2);
		}
		CauseExhaustion(0.1f);
	}
	else
	{
		World->PlaySound(TEXT("player_attack_nodamage"), Pos, 1.f, 1.f);
	}
	return bHit;
}

// ---------------------------------------------------------------------------------------------------------------------
// Using items

void AMCPlayer::TickUseItem()
{
	if (!World) return;
	const bool bPressed = Input.bUsePressed;
	const bool bHeld = Input.bUseHeld;
	Input.bUsePressed = false;
	if (IsMenuOpen() || Health <= 0.f || bSleeping)
	{
		if (bUsingItem) StopUsingItem(false);
		return;
	}
	if (bUsingItem)
	{
		const FMCItemStack& H = bUsingOffhand ? Inventory.Slots[MCInv::Offhand] : HeldConst();
		if (H.IsEmpty()) { StopUsingItem(false); return; }
		if (!bHeld) { StopUsingItem(true); return; }
		--UseItemRemaining;
		const FMCItem& I = H.Item();
		const int32 Elapsed = UseItemDuration - UseItemRemaining;
		if (I.Kind == EMCItemKind::Shield) bBlocking = Elapsed >= 5;
		if ((I.Food.IsValid() || I.Kind == EMCItemKind::Potion || I.Name == TEXT("milk_bucket") || I.Name == TEXT("honey_bottle")) && Elapsed > 6 && Elapsed % 4 == 0)
		{
			const bool bDrink = I.Kind == EMCItemKind::Potion || I.Name == TEXT("milk_bucket") || I.Name == TEXT("honey_bottle");
			PlaySound(bDrink ? TEXT("player_drink") : TEXT("player_eat"), 0.5f, 0.9f + Rand().NextFloat() * 0.2f);
			if (!bDrink) World->SpawnParticles(TEXT("item_crumbs"), GetEyePos() + GetLookDir() * 0.4 - FVector(0, 0, 0.15), 3, 0.08f, FVector(0, 0, 0.05), FColor::White);
		}
		if (I.Kind == EMCItemKind::Crossbow && Elapsed == FMath::Max(5, 25 - 5 * H.GetEnchant(EMCEnchant::QuickCharge)))
		{
			PlaySound(TEXT("crossbow_loading_end"), 1.f, 1.f);
		}
		if (UseItemRemaining <= 0) FinishUsingItem();
		return;
	}
	if (bPressed || (bHeld && UseDelay == 0))
	{
		HandleUse(bPressed, bHeld);
	}
}

void AMCPlayer::StartUsingItem(bool bOffhand, int32 Duration)
{
	bUsingItem = true;
	bUsingOffhand = bOffhand;
	UseItemDuration = Duration;
	UseItemRemaining = Duration;
	bBlocking = false;
	StopMining();
}

void AMCPlayer::StopUsingItem(bool bRelease)
{
	if (!bUsingItem) return;
	const int32 Used = UseItemDuration - UseItemRemaining;
	const bool bOff = bUsingOffhand;
	bUsingItem = false;
	bBlocking = false;
	UseItemRemaining = 0;
	if (!bRelease || !World || !Game) return;
	FMCItemStack& H = bOff ? Inventory.Slots[MCInv::Offhand] : Held();
	if (H.IsEmpty()) return;
	const FMCItem& I = H.Item();
	const FVector Eye = GetEyePos();
	const FVector Look = GetLookDir();
	if (I.Kind == EMCItemKind::Bow)
	{
		const float Power = BowPower(Used);
		if (Power < 0.1f) return;
		const bool bInfinity = H.GetEnchant(EMCEnchant::Infinity) > 0;
		int32 ArrowSlot = -1;
		for (int32 s : { (int32)MCInv::Offhand, Selected })
		{
			const FMCItemStack& A = Inventory.Slots[s];
			if (!A.IsEmpty() && (A.Item().Name == TEXT("arrow") || A.Item().Name == TEXT("spectral_arrow") || A.Item().Name == TEXT("tipped_arrow"))) { ArrowSlot = s; break; }
		}
		if (ArrowSlot < 0) for (int32 s = 0; s < 36; ++s)
		{
			const FMCItemStack& A = Inventory.Slots[s];
			if (!A.IsEmpty() && (A.Item().Name == TEXT("arrow") || A.Item().Name == TEXT("spectral_arrow") || A.Item().Name == TEXT("tipped_arrow"))) { ArrowSlot = s; break; }
		}
		if (ArrowSlot < 0 && !IsCreative()) return;
		const FMCItemStack Ammo = ArrowSlot >= 0 ? Inventory.Slots[ArrowSlot].Copy() : FMCItemStack::Of(TEXT("arrow"), 1);
		AMCProjectile* A = Game->SpawnEntity<AMCProjectile>(World, Eye - FVector(0, 0, 0.1));
		if (!A) return;
		A->Type = Ammo.Item().Name == TEXT("spectral_arrow") ? EMCProjectile::SpectralArrow : EMCProjectile::Arrow;
		A->Shooter = this;
		A->Item = Ammo.Copy(); A->Item.Count = 1;
		A->Damage = 2.f;
		const int32 PowerEnch = H.GetEnchant(EMCEnchant::Power);
		if (PowerEnch > 0) A->Damage += PowerEnch * 0.5f + 0.5f;
		A->Knockback = H.GetEnchant(EMCEnchant::Punch);
		A->bFlame = H.GetEnchant(EMCEnchant::Flame) > 0;
		A->bCritical = Power >= 1.f;
		A->bPickup = !IsCreative() && !(bInfinity && Ammo.Item().Name == TEXT("arrow"));
		A->bCreativePickup = IsCreative();
		A->InitEntity();
		A->Shoot(Look, Power * 3.f, 1.f);
		if (A->bFlame) A->SetOnFire(100);
		PlaySound(TEXT("bow_shoot"), 1.f, 1.f / (Rand().NextFloat() * 0.4f + 1.2f) + Power * 0.5f);
		DamageHeld(1, bOff);
		if (ArrowSlot >= 0 && !IsCreative() && !(bInfinity && Ammo.Item().Name == TEXT("arrow")))
		{
			FMCItemStack& AS = Inventory.Slots[ArrowSlot];
			if (--AS.Count <= 0) AS.Clear();
		}
	}
	else if (I.Kind == EMCItemKind::Trident)
	{
		if (Used < 10) return;
		const int32 Riptide = H.GetEnchant(EMCEnchant::Riptide);
		if (Riptide > 0)
		{
			if (!(bInWater || (Game->IsRainingAt(BlockPos()) && World->CanSeeSky(BlockPos())))) return;
			const double Str = 3.0 * (1.0 + Riptide) / 4.0;
			Vel += Look * Str;
			if (bOnGround) Vel.Z += 1.1999999;
			PlaySound(Riptide >= 3 ? TEXT("trident_riptide_3") : TEXT("trident_riptide_1"), 1.f, 1.f);
			DamageHeld(1, bOff);
			return;
		}
		AMCProjectile* T = Game->SpawnEntity<AMCProjectile>(World, Eye - FVector(0, 0, 0.1));
		if (!T) return;
		T->Type = EMCProjectile::Trident;
		T->Shooter = this;
		T->Item = H.Copy();
		T->Damage = 8.f;
		T->Loyalty = H.GetEnchant(EMCEnchant::Loyalty);
		T->bPickup = !IsCreative();
		T->bCreativePickup = IsCreative();
		T->InitEntity();
		T->Shoot(Look, 2.5f, 1.f);
		PlaySound(TEXT("trident_throw"), 1.f, 1.f);
		T->Item.DamageItem(1, Rand());
		if (!IsCreative()) H.Clear();
	}
	else if (I.Kind == EMCItemKind::Crossbow)
	{
		const int32 Charge = FMath::Max(5, 25 - 5 * H.GetEnchant(EMCEnchant::QuickCharge));
		if (Used >= Charge && !(H.Extra.IsValid() && H.Extra->Charge > 0))
		{
			// load ammunition (firework in offhand has priority)
			FName Ammo = NAME_None;
			int32 Slot = -1;
			const FMCItemStack& Off = Inventory.Slots[MCInv::Offhand];
			if (!Off.IsEmpty() && Off.Item().Name == TEXT("firework_rocket")) { Ammo = TEXT("firework_rocket"); Slot = MCInv::Offhand; }
			if (Slot < 0) for (int32 s = 0; s < 41; ++s)
			{
				const FMCItemStack& A = Inventory.Slots[s];
				if (!A.IsEmpty() && (A.Item().Name == TEXT("arrow") || A.Item().Name == TEXT("spectral_arrow") || A.Item().Name == TEXT("tipped_arrow"))) { Ammo = A.Item().Name; Slot = s; break; }
			}
			if (Slot < 0 && IsCreative()) Ammo = TEXT("arrow");
			if (Ammo.IsNone()) return;
			FMCItemExtra& X = H.MutableExtra();
			X.Charge = H.GetEnchant(EMCEnchant::Multishot) > 0 ? 3 : 1;
			X.Loaded = Ammo;
			if (Slot >= 0 && !IsCreative()) { FMCItemStack& AS = Inventory.Slots[Slot]; if (--AS.Count <= 0) AS.Clear(); }
			PlaySound(TEXT("crossbow_loading_end"), 1.f, 1.f);
		}
	}
	else if (I.Kind == EMCItemKind::Spear)
	{
		// charged lunge: dash forward and damage what is in front
		if (Used < 8) return;
		const int32 Lunge = H.GetEnchant(EMCEnchant::Lunge);
		Vel += FVector(Look.X, Look.Y, 0.f).GetSafeNormal() * (0.6 + 0.3 * Lunge);
		TArray<AMCEntity*> Around;
		World->GetEntitiesInBox(GetBox().Offset(Look * 2.0).Inflate(1.0), Around, this);
		for (AMCEntity* O : Around) if (O->IsLiving()) { O->Hurt(FMCDamage::PlayerAttack(this), I.AttackDamage + (float)FMath::Min(8.0, Vel.Size() * 6.0)); }
		PlaySound(TEXT("spear_lunge"), 1.f, 1.f);
		DamageHeld(1, bOff);
	}
}

void AMCPlayer::FinishUsingItem()
{
	FMCItemStack& H = bUsingOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	const bool bOff = bUsingOffhand;
	bUsingItem = false;
	bBlocking = false;
	if (H.IsEmpty() || !World) return;
	const FMCItem I = H.Item();
	if (I.Food.IsValid())
	{
		EatFood(I);
		if (I.Name == TEXT("honey_bottle")) RemoveEffect(EMCEffect::Poison);
		if (I.Name == TEXT("suspicious_stew")) { FMCEffectInstance E; E.Effect = EMCEffect::Regeneration; E.Duration = 160; AddEffect(E); }
		if (!IsCreative())
		{
			if (!I.Food->Remainder.IsNone()) ReplaceHeld(FMCItemStack::Of(I.Food->Remainder, 1), bOff);
			else ConsumeHeld(1, bOff);
		}
		return;
	}
	if (I.Name == TEXT("milk_bucket"))
	{
		ClearEffects();
		PlaySound(TEXT("player_burp"), 0.5f, 1.f);
		if (!IsCreative()) ReplaceHeld(FMCItemStack::Of(TEXT("bucket"), 1), bOff);
		return;
	}
	if (I.Kind == EMCItemKind::Potion && I.Name == TEXT("potion"))
	{
		const int32 PI_ = H.Extra.IsValid() ? H.Extra->Potion : 0;
		const MCPotions::FPotionDef& D = MCPotions::Get(PI_);
		if (D.Effect != EMCEffect::None)
		{
			FMCEffectInstance E; E.Effect = D.Effect; E.Duration = D.Duration; E.Amplifier = D.Amplifier;
			AddEffect(E);
			if (FCString::Strcmp(D.Id, TEXT("turtle_master")) == 0) { FMCEffectInstance S; S.Effect = EMCEffect::Slowness; S.Duration = D.Duration; S.Amplifier = 3; AddEffect(S); }
		}
		if (FCString::Strcmp(D.Id, TEXT("water")) == 0) Extinguish();
		if (!IsCreative()) ReplaceHeld(FMCItemStack::Of(TEXT("glass_bottle"), 1), bOff);
		return;
	}
}

void AMCPlayer::HandleUse(bool bPressed, bool bHeld)
{
	if (!World || IsSpectator()) return;
	UseDelay = 4;
	FMCRayHit Hit;
	AMCEntity* Target = nullptr;
	GetTarget(Hit, Target);

	for (int32 HandIdx = 0; HandIdx < 2; ++HandIdx)
	{
		const bool bOff = HandIdx == 1;
		FMCItemStack& H = bOff ? Inventory.Slots[MCInv::Offhand] : Held();
		if (Target)
		{
			if (Target->Interact(this, bOff)) { Swing(); return; }
			// spawn egg on a matching mob spawns a baby
			if (!H.IsEmpty() && H.Item().Kind == EMCItemKind::SpawnEgg)
			{
				if (AMCMob* M = Cast<AMCMob>(Target))
				{
					if (M->Def && M->Def->Id == H.Item().SpawnMob && M->Def->bHasBaby && Game)
					{
						if (AMCMob* Baby = Cast<AMCMob>(Game->SpawnMob(World, M->Def->Id, M->Pos, false)))
						{
							Baby->SetBaby(true);
							ConsumeHeld(1, bOff);
							Swing();
							return;
						}
					}
				}
			}
			continue;
		}
		if (Hit.bHit)
		{
			const FMCBlock& B = FMCBlocks::GetByState(Hit.State);
			const bool bBothEmpty = Held().IsEmpty() && Inventory.Slots[MCInv::Offhand].IsEmpty();
			if (HandIdx == 0 && (!bSneaking || bBothEmpty) && GameMode != EMCGameMode::Spectator)
			{
				if (B.Behavior->OnUse(*World, Hit.Pos, Hit.State, this, Hit.Face, Hit.HitFrac)) { Swing(); return; }
			}
			if (UseItemOn(Hit, bOff)) return;
		}
		if (bPressed || bHeld)
		{
			if (UseItemInAir(bOff)) return;
		}
	}
}

bool AMCPlayer::UseItemOn(const FMCRayHit& Hit, bool bOffhand)
{
	FMCItemStack& H = bOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	if (H.IsEmpty() || !World || !Game) return false;
	if (GameMode == EMCGameMode::Adventure && H.Item().Block) return false;
	const FMCItem& I = H.Item();
	const FMCBlockPos P = Hit.Pos;
	const FMCState S = Hit.State;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	const FMCBlockPos Adj = P.Offset(Hit.Face);

	// --- tools that convert blocks
	if (I.ToolType == EMCTool::Axe)
	{
		FName To = StrippedOf(B.Name);
		FName Sound = TEXT("axe_strip");
		if (To.IsNone()) { To = ScrapedOf(B.Name); Sound = B.Name.ToString().StartsWith(TEXT("waxed_")) ? FName(TEXT("axe_wax_off")) : FName(TEXT("axe_scrape")); }
		if (!To.IsNone())
		{
			const FMCBlock* NB = FMCBlocks::Find(To);
			World->SetState(P, NB->State(FMCBlocks::MetaOf(S)), MCSet_Default | MCSet_KeepEntity);
			World->PlaySound(Sound, FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 1.f, 1.f);
			if (Sound != TEXT("axe_strip")) World->SpawnParticles(Sound == TEXT("axe_wax_off") ? TEXT("wax_off") : TEXT("scrape"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 12, 0.6f);
			DamageHeld(1, bOffhand);
			Swing();
			return true;
		}
	}
	if (I.ToolType == EMCTool::Shovel && Hit.Face != EMCFace::Down && World->GetState(P.Up()) == 0)
	{
		static const TCHAR* Pathable[] = { TEXT("grass_block"), TEXT("dirt"), TEXT("podzol"), TEXT("mycelium"), TEXT("coarse_dirt"), TEXT("rooted_dirt") };
		for (const TCHAR* N : Pathable)
		{
			if (B.Name != FName(N)) continue;
			World->SetState(P, MCBeh::StateOf(TEXT("dirt_path")), MCSet_Default);
			World->PlaySound(TEXT("shovel_flatten"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 1.0), 1.f, 1.f);
			DamageHeld(1, bOffhand);
			Swing();
			return true;
		}
	}
	if (I.ToolType == EMCTool::Hoe && Hit.Face != EMCFace::Down && World->GetState(P.Up()) == 0)
	{
		FMCState To = 0;
		if (B.Name == TEXT("grass_block") || B.Name == TEXT("dirt") || B.Name == TEXT("dirt_path")) To = MCBeh::StateOf(TEXT("farmland"));
		else if (B.Name == TEXT("coarse_dirt")) To = MCBeh::StateOf(TEXT("dirt"));
		else if (B.Name == TEXT("rooted_dirt")) { To = MCBeh::StateOf(TEXT("dirt")); MCBehaviorUtil::PopItem(*World, P.Up(), FMCItemStack::Of(TEXT("hanging_roots"), 1)); }
		if (To)
		{
			World->SetState(P, To, MCSet_Default);
			World->PlaySound(TEXT("hoe_till"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 1.0), 1.f, 1.f);
			DamageHeld(1, bOffhand);
			Swing();
			return true;
		}
	}
	if (I.Name == TEXT("honeycomb") && B.Name.ToString().Contains(TEXT("copper")) && !B.Name.ToString().StartsWith(TEXT("waxed_")))
	{
		const FName Waxed(*(TEXT("waxed_") + B.Name.ToString()));
		if (const FMCBlock* NB = FMCBlocks::Find(Waxed))
		{
			World->SetState(P, NB->State(FMCBlocks::MetaOf(S)), MCSet_Default | MCSet_KeepEntity);
			World->PlaySound(TEXT("copper_wax_on"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 1.f, 1.f);
			World->SpawnParticles(TEXT("wax_on"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 12, 0.6f);
			ConsumeHeld(1, bOffhand);
			Swing();
			return true;
		}
	}

	// --- ignition
	if (I.Kind == EMCItemKind::FlintAndSteel || I.Kind == EMCItemKind::FireCharge)
	{
		if (B.Behavior->OnIgnite(*World, P, S, this))
		{
			if (I.Kind == EMCItemKind::FlintAndSteel) DamageHeld(1, bOffhand); else ConsumeHeld(1, bOffhand);
			Swing();
			return true;
		}
		if (World->GetState(Adj) == 0)
		{
			World->PlaySound(I.Kind == EMCItemKind::FlintAndSteel ? TEXT("flint_and_steel_use") : TEXT("fire_charge_use"), FVector(Adj.X + 0.5, Adj.Y + 0.5, Adj.Z + 0.5), 1.f, 0.8f + Rand().NextFloat() * 0.4f);
			if (!Game->TryCreateNetherPortal(*World, Adj))
			{
				const bool bSoul = World->Dim != EMCDimension::End && (FMCBlocks::GetByState(World->GetState(Adj.Down())).Name == TEXT("soul_sand") || FMCBlocks::GetByState(World->GetState(Adj.Down())).Name == TEXT("soul_soil"));
				World->SetState(Adj, bSoul ? MCBeh::StateOf(TEXT("soul_fire")) : FMCBlocks::C.Fire, MCSet_Default);
			}
			if (I.Kind == EMCItemKind::FlintAndSteel) DamageHeld(1, bOffhand); else ConsumeHeld(1, bOffhand);
			Swing();
			return true;
		}
	}

	// --- bone meal
	if (I.Kind == EMCItemKind::BoneMeal)
	{
		if (MCBeh::ApplyBoneMeal(*World, P, this))
		{
			ConsumeHeld(1, bOffhand);
			World->SpawnParticles(TEXT("happy_villager"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 15, 0.6f);
			World->PlaySound(TEXT("bone_meal_use"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 1.f, 1.f);
			Swing();
			return true;
		}
		return false;
	}

	// --- spawn eggs on blocks
	if (I.Kind == EMCItemKind::SpawnEgg)
	{
		const FVector SpawnAt(Adj.X + 0.5, Adj.Y + 0.5, Adj.Z + (FMCBlocks::IsSolid(World->GetState(Adj)) ? 1.0 : 0.0));
		if (AMCEntity* E = Game->SpawnMob(World, I.SpawnMob, SpawnAt, false))
		{
			E->bPersistent = true;
			E->Yaw = ViewYaw + 180.f;
			if (!H.Extra.IsValid() || H.Extra->CustomName.IsEmpty()) {}
			else E->CustomName = H.Extra->CustomName;
			ConsumeHeld(1, bOffhand);
			Swing();
			return true;
		}
		return false;
	}

	// --- buckets / bottles / vehicles / crystals / eyes / fireworks on blocks
	if (I.Kind == EMCItemKind::Bucket || I.Kind == EMCItemKind::Bottle) return UseItemInAir(bOffhand);
	if (I.Kind == EMCItemKind::Boat)
	{
		const FVector At = Hit.Point + FVector(0, 0, 0.05);
		AMCBoat* Bt = Game->SpawnEntity<AMCBoat>(World, At);
		if (!Bt) return false;
		Bt->Wood = I.Family.IsNone() ? FName(TEXT("oak")) : I.Family;
		Bt->bChest = I.Name.ToString().Contains(TEXT("chest"));
		Bt->Yaw = ViewYaw;
		Bt->InitEntity();
		ConsumeHeld(1, bOffhand);
		Swing();
		return true;
	}
	if (I.Kind == EMCItemKind::Minecart)
	{
		if (B.Model != EMCModel::Rail) return false;
		AMCMinecart* M = Game->SpawnEntity<AMCMinecart>(World, FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.0625));
		if (!M) return false;
		M->Variant = I.Name;
		M->InitEntity();
		ConsumeHeld(1, bOffhand);
		Swing();
		return true;
	}
	if (I.Kind == EMCItemKind::EndCrystal)
	{
		if ((B.Name != TEXT("obsidian") && B.Name != TEXT("bedrock")) || Hit.Face != EMCFace::Up) return false;
		if (World->GetState(P.Up()) != 0 || World->GetState(P.Up(2)) != 0) return false;
		TArray<AMCEntity*> Occ;
		World->GetEntitiesInBox(FMCBox(FVector(P.X, P.Y, P.Z + 1), FVector(P.X + 1, P.Y + 1, P.Z + 3)), Occ, nullptr);
		if (Occ.Num() > 0) return false;
		if (AMCEndCrystal* C = Game->SpawnEntity<AMCEndCrystal>(World, FVector(P.X + 0.5, P.Y + 0.5, P.Z + 1)))
		{
			C->bShowBottom = false;
			ConsumeHeld(1, bOffhand);
			Swing();
			Game->TickDragonFight();
			return true;
		}
		return false;
	}
	if (I.Kind == EMCItemKind::Firework)
	{
		if (AMCFirework* F = Game->SpawnEntity<AMCFirework>(World, Hit.Point + FVector(0, 0, 0.05)))
		{
			F->Item = H.Copy();
			F->LifeTime = 10 * (H.Extra.IsValid() ? FMath::Max<int32>(1, H.Extra->FlightDuration) : 1) + Rand().NextInt(6) + Rand().NextInt(7);
			ConsumeHeld(1, bOffhand);
			Swing();
			return true;
		}
	}
	if (I.Name == TEXT("item_frame") || I.Name == TEXT("glow_item_frame"))
	{
		if (!MC::IsHorizontal(Hit.Face) && Hit.Face != EMCFace::Up && Hit.Face != EMCFace::Down) return false;
		if (World->GetState(Adj) != 0) return false;
		if (AMCItemFrame* F = Game->SpawnEntity<AMCItemFrame>(World, FVector(Adj.X + 0.5, Adj.Y + 0.5, Adj.Z)))
		{
			F->Facing = Hit.Face;
			F->bGlow = I.Name == TEXT("glow_item_frame");
			F->InitEntity();
			ConsumeHeld(1, bOffhand);
			PlaySound(TEXT("item_frame_place"), 1.f, 1.f);
			Swing();
			return true;
		}
	}
	if (I.Kind == EMCItemKind::Brush)
	{
		if (B.Name == TEXT("suspicious_sand") || B.Name == TEXT("suspicious_gravel"))
		{
			World->PlaySound(B.Name == TEXT("suspicious_sand") ? TEXT("brush_sand") : TEXT("brush_gravel"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 1.f, 1.f);
			World->SpawnBlockBreakParticles(P, S);
			if (Rand().NextInt(8) == 0)
			{
				FMCContainer Loot; Loot.Init(1);
				MCLoot::FillContainer(B.Name == TEXT("suspicious_sand") ? FName(TEXT("archaeology_desert")) : FName(TEXT("archaeology_trail")), World->Seed ^ (uint64)GetTypeHash(P), Loot);
				if (!Loot[0].IsEmpty()) MCBehaviorUtil::PopItem(*World, Adj, Loot[0]);
				World->SetState(P, B.Name == TEXT("suspicious_sand") ? FMCBlocks::C.Sand : FMCBlocks::C.Gravel, MCSet_Default);
				DamageHeld(1, bOffhand);
			}
			Swing();
			return true;
		}
	}

	// --- block placement
	if (I.Block && I.Kind != EMCItemKind::SpawnEgg)
	{
		return TryPlaceBlock(Adj, Hit, H, bOffhand);
	}
	return false;
}

bool AMCPlayer::TryPlaceBlock(const FMCBlockPos& InTarget, const FMCRayHit& Hit, const FMCItemStack& Stack, bool bOffhand)
{
	if (!World || Stack.IsEmpty()) return false;
	const FMCItem& I = Stack.Item();
	const FMCBlock& B = FMCBlocks::Get(I.Block);
	FMCPlaceContext Ctx;
	Ctx.World = World;
	Ctx.Player = this;
	Ctx.ClickedPos = Hit.Pos;
	Ctx.ClickedFace = Hit.Face;
	Ctx.HitFrac = Hit.HitFrac;
	Ctx.Yaw = ViewYaw;
	Ctx.Pitch = ViewPitch;
	Ctx.bSneaking = bSneaking;

	FMCBlockPos Target = InTarget;
	FMCState Place = 0;
	const FMCState ClickedS = Hit.State;
	const FMCBlock& ClickedB = FMCBlocks::GetByState(ClickedS);
	// 1) merge into the clicked block (slabs, snow layers, candles...)
	if (ClickedB.Id == B.Id && IsMergeModel(B))
	{
		Ctx.Pos = Hit.Pos;
		Ctx.Existing = ClickedS;
		const FMCState M = B.Behavior->GetPlacementState(B, Ctx);
		if (M != 0 && M != ClickedS) { Target = Hit.Pos; Place = M; }
	}
	// 2) replace the clicked block (grass, water, a single snow layer...)
	if (!Place && FMCBlocks::IsReplaceable(ClickedS) && ClickedB.Id != B.Id)
	{
		Target = Hit.Pos;
	}
	if (!Place)
	{
		const FMCState Existing = World->GetState(Target);
		const FMCBlock& EB = FMCBlocks::GetByState(Existing);
		Ctx.Pos = Target;
		Ctx.Existing = Existing;
		if (Existing != 0 && !FMCBlocks::IsReplaceable(Existing) && !(EB.Id == B.Id && IsMergeModel(B))) return false;
		if (EB.Id == B.Id && !IsMergeModel(B) && !FMCBlocks::IsReplaceable(Existing)) return false;
		Place = B.Behavior->GetPlacementState(B, Ctx);
		if (Place == 0 || Place == Existing) return false;
	}
	if (!FMCChunk::InRange(Target.Z) || !World->IsReadyAt(Target)) return false;
	if (Target.Z >= MC::MaxZ + 1) { ShowActionBar(TEXT("Height limit for building is 320")); return false; }

	// entities in the way
	TArray<FMCBox> Col;
	FMCBlocks::GetCollision(Place, World, Target, Col);
	if (Col.Num() > 0)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(FMCBox(FVector(Target.X, Target.Y, Target.Z), FVector(Target.X + 1, Target.Y + 1, Target.Z + 1)).Inflate(0.01), Near, nullptr);
		for (AMCEntity* E : Near)
		{
			if (!E->IsLiving() && !E->IsA<AMCBoat>() && !E->IsA<AMCMinecart>()) continue;
			if (const AMCPlayer* PL = Cast<AMCPlayer>(E)) if (PL->IsSpectator()) continue;
			const FMCBox EB = E->GetBox();
			for (const FMCBox& C : Col) if (C.Offset(Target.X, Target.Y, Target.Z).Intersects(EB)) return false;
		}
	}
	if (!B.Behavior->CanSurvive(B, *World, Target, Place)) return false;
	if (!World->PlaceBlock(Target, Place, this)) return false;
	++StatBlocksPlaced;
	// block item carries container data (shulker boxes)
	if (Stack.Extra.IsValid() && Stack.Extra->BlockEntityData.Num() > 0)
	{
		if (FMCBlockEntity* BE = World->GetBlockEntity(Target))
		{
			if (FMCContainer* C = BE->GetContainer())
			{
				TArray<uint8> Data = Stack.Extra->BlockEntityData;
				FMemoryReader Ar(Data);
				C->Serialize(Ar);
			}
		}
	}
	if (Stack.Extra.IsValid() && !Stack.Extra->CustomName.IsEmpty())
	{
		if (FMCBlockEntity* BE = World->GetBlockEntity(Target)) BE->CustomName = Stack.Extra->CustomName;
	}
	ConsumeHeld(1, bOffhand);
	Swing();
	// Wither summoning
	if (B.Name == TEXT("wither_skeleton_skull") || B.Name == TEXT("wither_skeleton_wall_skull")) AMCWither::TrySpawnFromStructure(*World, Target);
	// iron / snow golems from carved pumpkins
	if (B.Name == TEXT("carved_pumpkin") || B.Name == TEXT("jack_o_lantern"))
	{
		const FName Below1 = FMCBlocks::GetByState(World->GetState(Target.Down())).Name;
		const FName Below2 = FMCBlocks::GetByState(World->GetState(Target.Down(2))).Name;
		if (Below1 == TEXT("snow_block") && Below2 == TEXT("snow_block") && Game)
		{
			World->SetState(Target, 0); World->SetState(Target.Down(), 0); World->SetState(Target.Down(2), 0);
			Game->SpawnMob(World, TEXT("snow_golem"), FVector(Target.X + 0.5, Target.Y + 0.5, Target.Z - 2), false);
		}
		else if (Below1 == TEXT("iron_block") && Below2 == TEXT("iron_block") && Game)
		{
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const FMCBlockPos A = Axis == 0 ? FMCBlockPos(Target.X - 1, Target.Y, Target.Z - 1) : FMCBlockPos(Target.X, Target.Y - 1, Target.Z - 1);
				const FMCBlockPos Bp = Axis == 0 ? FMCBlockPos(Target.X + 1, Target.Y, Target.Z - 1) : FMCBlockPos(Target.X, Target.Y + 1, Target.Z - 1);
				if (FMCBlocks::GetByState(World->GetState(A)).Name == TEXT("iron_block") && FMCBlocks::GetByState(World->GetState(Bp)).Name == TEXT("iron_block"))
				{
					for (const FMCBlockPos& C : { Target, Target.Down(), Target.Down(2), A, Bp }) World->SetState(C, 0);
					Game->SpawnMob(World, TEXT("iron_golem"), FVector(Target.X + 0.5, Target.Y + 0.5, Target.Z - 2), false);
					break;
				}
			}
		}
		else if (Below1 == TEXT("copper_block") && Game)
		{
			World->SetState(Target, 0); World->SetState(Target.Down(), 0);
			Game->SpawnMob(World, TEXT("copper_golem"), FVector(Target.X + 0.5, Target.Y + 0.5, Target.Z - 1), false);
		}
	}
	return true;
}

void AMCPlayer::PlaceBlockFromItem(const FMCRayHit& Hit, const FMCItemStack& Stack, bool bOffhand)
{
	TryPlaceBlock(Hit.Pos.Offset(Hit.Face), Hit, Stack, bOffhand);
}

bool AMCPlayer::UseItemInAir(bool bOffhand)
{
	FMCItemStack& H = bOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	if (H.IsEmpty() || !World || !Game) return false;
	if (ItemCooldown > 0 && (H.Item().Kind == EMCItemKind::EnderPearl || H.Item().Kind == EMCItemKind::Shield || H.Item().Name == TEXT("chorus_fruit") || H.Item().Kind == EMCItemKind::Horn)) return false;
	const FMCItem& I = H.Item();
	const FVector Eye = GetEyePos();
	const FVector Look = GetLookDir();

	auto Throw = [&](EMCProjectile Type, float Speed, float Inacc) -> AMCProjectile*
	{
		AMCProjectile* P = Game->SpawnEntity<AMCProjectile>(World, Eye - FVector(0, 0, 0.1));
		if (!P) return nullptr;
		P->Type = Type;
		P->Shooter = this;
		P->Item = H.Copy(); P->Item.Count = 1;
		P->InitEntity();
		P->Shoot(Look, Speed, Inacc);
		P->Vel += FVector(Vel.X, Vel.Y, bOnGround ? 0.0 : Vel.Z);
		ConsumeHeld(1, bOffhand);
		Swing();
		return P;
	};

	// food & drink
	if (I.Food.IsValid() && I.Kind != EMCItemKind::Placeable)
	{
		if (!CanEat(I.Food->bAlwaysEdible)) return false;
		StartUsingItem(bOffhand, FMath::RoundToInt(I.Food->EatSeconds * 20.f));
		return true;
	}
	if (I.Food.IsValid() && I.Kind == EMCItemKind::Food && I.Block)
	{
		if (!CanEat(false)) return false;
		StartUsingItem(bOffhand, 32);
		return true;
	}
	switch (I.Kind)
	{
	case EMCItemKind::Potion:
		if (I.Name == TEXT("potion")) { StartUsingItem(bOffhand, 32); return true; }
		{
			AMCProjectile* P = Throw(I.Name == TEXT("lingering_potion") ? EMCProjectile::LingeringPotion : EMCProjectile::Potion, 0.5f, 1.f);
			if (P) { P->Vel.Z += 0.1; PlaySound(TEXT("splash_potion_throw"), 0.5f, 0.4f / (Rand().NextFloat() * 0.4f + 0.8f)); }
			return P != nullptr;
		}
	case EMCItemKind::Bow:
	{
		bool bAmmo = IsCreative() || H.GetEnchant(EMCEnchant::Infinity) > 0;
		for (int32 s = 0; s < 41 && !bAmmo; ++s)
		{
			const FMCItemStack& A = Inventory.Slots[s];
			if (!A.IsEmpty() && (A.Item().Name == TEXT("arrow") || A.Item().Name == TEXT("spectral_arrow") || A.Item().Name == TEXT("tipped_arrow"))) bAmmo = true;
		}
		if (!bAmmo) return false;
		StartUsingItem(bOffhand, 72000);
		return true;
	}
	case EMCItemKind::Crossbow:
	{
		if (H.Extra.IsValid() && H.Extra->Charge > 0)
		{
			// fire
			const int32 N = H.Extra->Charge;
			const FName Ammo = H.Extra->Loaded;
			for (int32 k = 0; k < N; ++k)
			{
				const float Spread = N > 1 ? (k - 1) * 10.f : 0.f;
				const FVector Dir = FRotator(ViewPitch, ViewYaw + Spread, 0).Vector();
				if (Ammo == TEXT("firework_rocket"))
				{
					if (AMCFirework* F = Game->SpawnEntity<AMCFirework>(World, Eye))
					{
						F->Vel = Dir * 1.6;
						F->LifeTime = 30;
						F->Item = FMCItemStack::Of(TEXT("firework_rocket"), 1);
					}
					continue;
				}
				AMCProjectile* A = Game->SpawnEntity<AMCProjectile>(World, Eye - FVector(0, 0, 0.1));
				if (!A) continue;
				A->Type = Ammo == TEXT("spectral_arrow") ? EMCProjectile::SpectralArrow : EMCProjectile::Arrow;
				A->Shooter = this;
				A->Item = FMCItemStack::Of(Ammo, 1);
				A->Damage = 2.f + 1.f;
				A->Pierce = H.GetEnchant(EMCEnchant::Piercing);
				A->bPickup = k == 0 && !IsCreative();
				A->bCritical = true;
				A->InitEntity();
				A->Shoot(Dir, 3.15f, 1.f);
			}
			H.MutableExtra().Charge = 0;
			H.MutableExtra().Loaded = NAME_None;
			PlaySound(TEXT("crossbow_shoot"), 1.f, 1.f);
			DamageHeld(N > 1 ? 3 : 1, bOffhand);
			Swing();
			return true;
		}
		StartUsingItem(bOffhand, 72000);
		PlaySound(TEXT("crossbow_loading_start"), 1.f, 1.f);
		return true;
	}
	case EMCItemKind::Trident:
	case EMCItemKind::Spear:
		StartUsingItem(bOffhand, 72000);
		return true;
	case EMCItemKind::Shield:
		StartUsingItem(bOffhand, 72000);
		return true;
	case EMCItemKind::Spyglass:
		StartUsingItem(bOffhand, 1200);
		PlaySound(TEXT("spyglass_use"), 1.f, 1.f);
		return true;
	case EMCItemKind::Horn:
		PlaySound(TEXT("goat_horn_sound"), 4.f, 1.f);
		ItemCooldown = 140;
		return true;
	case EMCItemKind::Snowball: { const bool b = Throw(EMCProjectile::Snowball, 1.5f, 1.f) != nullptr; PlaySound(TEXT("snowball_throw"), 0.5f, 0.4f); return b; }
	case EMCItemKind::Egg: { const bool b = Throw(EMCProjectile::Egg, 1.5f, 1.f) != nullptr; PlaySound(TEXT("egg_throw"), 0.5f, 0.4f); return b; }
	case EMCItemKind::EnderPearl: { const bool b = Throw(EMCProjectile::EnderPearl, 1.5f, 1.f) != nullptr; ItemCooldown = 20; PlaySound(TEXT("ender_pearl_throw"), 0.5f, 0.4f); return b; }
	case EMCItemKind::ExpBottle: { AMCProjectile* P = Throw(EMCProjectile::ExpBottle, 0.7f, 1.f); if (P) P->Vel.Z += 0.2; PlaySound(TEXT("experience_bottle_throw"), 0.5f, 0.4f); return P != nullptr; }
	case EMCItemKind::Projectile:
		if (I.Name == TEXT("wind_charge")) { const bool b = Throw(EMCProjectile::WindCharge, 1.5f, 1.f) != nullptr; ItemCooldown = 10; PlaySound(TEXT("wind_charge_throw"), 0.5f, 0.4f); return b; }
		return false;
	case EMCItemKind::EyeOfEnder:
	{
		FMCBlockPos Found;
		if (World->Generator && World->Generator->LocateStructure(TEXT("stronghold"), BlockPos(), 200, Found))
		{
			if (AMCEyeOfEnder* E = Game->SpawnEntity<AMCEyeOfEnder>(World, Eye - FVector(0, 0, 0.1)))
			{
				E->TargetPos = FVector(Found.X + 0.5, Found.Y + 0.5, Found.Z);
				E->bSurvive = Rand().NextInt(5) > 0;
				ConsumeHeld(1, bOffhand);
				PlaySound(TEXT("ender_eye_launch"), 0.5f, 0.4f);
				Swing();
				return true;
			}
		}
		return false;
	}
	case EMCItemKind::Firework:
		if (bElytraFlying)
		{
			if (AMCFirework* F = Game->SpawnEntity<AMCFirework>(World, Pos))
			{
				F->AttachedTo = this;
				F->Item = H.Copy();
				F->LifeTime = 10 * (H.Extra.IsValid() ? FMath::Max<int32>(1, H.Extra->FlightDuration) : 1) + Rand().NextInt(6);
				ConsumeHeld(1, bOffhand);
				return true;
			}
		}
		return false;
	case EMCItemKind::FishingRod:
	{
		// reel in an existing bobber
		for (AMCEntity* E : World->Entities)
		{
			AMCProjectile* B = Cast<AMCProjectile>(E);
			if (B && B->Type == EMCProjectile::FishingBobber && B->Shooter.Get() == this && !B->bRemoved)
			{
				if (B->HomingTarget.IsValid())
				{
					// pull the hooked entity
					AMCEntity* T = B->HomingTarget.Get();
					T->Vel += (Pos - T->Pos) * 0.1;
				}
				else if (B->Shake > 0)
				{
					const FMCItemStack Catch = MCLoot::Fish(H.GetEnchant(EMCEnchant::LuckOfTheSea), true, Rand());
					if (AMCItemEntity* IE = Game->SpawnEntity<AMCItemEntity>(World, B->Pos))
					{
						IE->SetStack(Catch);
						const FVector D = (GetEyePos() - B->Pos);
						IE->Vel = FVector(D.X * 0.1, D.Y * 0.1, D.Z * 0.1 + FMath::Sqrt(D.Size()) * 0.08);
					}
					World->SpawnXP(Pos, Rand().Range(1, 6));
				}
				PlaySound(TEXT("fishing_bobber_retrieve"), 1.f, 0.4f / (Rand().NextFloat() * 0.4f + 0.8f));
				B->Discard();
				DamageHeld(1, bOffhand);
				Swing();
				return true;
			}
		}
		AMCProjectile* B = Game->SpawnEntity<AMCProjectile>(World, Eye - FVector(0, 0, 0.1));
		if (!B) return false;
		B->Type = EMCProjectile::FishingBobber;
		B->Shooter = this;
		B->bPickup = false;
		B->InitEntity();
		B->Shoot(Look, 1.0f, 1.f);
		PlaySound(TEXT("fishing_bobber_throw"), 0.5f, 0.4f / (Rand().NextFloat() * 0.4f + 0.8f));
		Swing();
		return true;
	}
	case EMCItemKind::Bucket:
	case EMCItemKind::Bottle:
	{
		FMCRayHit FH;
		if (!World->Raycast(Eye, Look, GetReach(), FH, true, false)) return false;
		const FMCStateInfo& FI = FMCBlocks::Info(FH.State);
		const bool bSource = (FI.Flags & MCB_Fluid) && (FI.Meta & 15) == 0;
		if (I.Kind == EMCItemKind::Bottle)
		{
			if (FI.Block == FMCBlocks::C.WaterId || (FI.Flags & MCB_Waterlogged))
			{
				FMCItemStack Water = FMCItemStack::Of(TEXT("potion"), 1);
				Water.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(TEXT("water")));
				ReplaceHeld(Water, bOffhand);
				PlaySound(TEXT("bottle_fill"), 1.f, 1.f);
				Swing();
				return true;
			}
			return false;
		}
		if (I.Fluid.IsNone())
		{
			// empty bucket: pick up a source
			if (bSource && (FI.Block == FMCBlocks::C.WaterId || FI.Block == FMCBlocks::C.LavaId))
			{
				World->SetState(FH.Pos, 0, MCSet_Default);
				const bool bLava = FI.Block == FMCBlocks::C.LavaId;
				ReplaceHeld(FMCItemStack::Of(bLava ? TEXT("lava_bucket") : TEXT("water_bucket"), 1), bOffhand);
				PlaySound(bLava ? TEXT("bucket_fill_lava") : TEXT("bucket_fill"), 1.f, 1.f);
				Swing();
				return true;
			}
			if (FMCBlocks::GetByState(FH.State).Name == TEXT("powder_snow"))
			{
				World->SetState(FH.Pos, 0, MCSet_Default);
				ReplaceHeld(FMCItemStack::Of(TEXT("powder_snow_bucket"), 1), bOffhand);
				PlaySound(TEXT("bucket_fill_powder_snow"), 1.f, 1.f);
				Swing();
				return true;
			}
			return false;
		}
		if (I.Name == TEXT("milk_bucket")) { StartUsingItem(bOffhand, 32); return true; }
		// place fluid: into the clicked cell if replaceable, else in front of the face
		FMCRayHit SH;
		if (!World->Raycast(Eye, Look, GetReach(), SH, false, false) && !(FI.Flags & MCB_Fluid)) return false;
		const FMCRayHit& Use = SH.bHit ? SH : FH;
		FMCBlockPos T = FMCBlocks::IsReplaceable(Use.State) ? Use.Pos : Use.Pos.Offset(Use.Face);
		const FMCState At = World->GetState(T);
		if (At != 0 && !FMCBlocks::IsReplaceable(At)) return false;
		if (I.Fluid == TEXT("water") && World->Dim == EMCDimension::Nether)
		{
			World->PlaySound(TEXT("fire_extinguish"), FVector(T.X + 0.5, T.Y + 0.5, T.Z + 0.5), 0.5f, 2.6f);
			World->SpawnParticles(TEXT("smoke_large"), FVector(T.X + 0.5, T.Y + 0.5, T.Z + 0.5), 8, 0.5f);
		}
		else
		{
			if (At != 0) World->DestroyBlock(T, true, this, nullptr, false);
			const FMCState FS = I.Fluid == TEXT("lava") ? FMCBlocks::C.Lava : (I.Fluid == TEXT("powder_snow") ? MCBeh::StateOf(TEXT("powder_snow")) : FMCBlocks::C.Water);
			World->SetState(T, FS, MCSet_Default);
			World->ScheduleTick(T, FMCBlocks::BlockOf(FS), 1);
			PlaySound(I.Fluid == TEXT("lava") ? TEXT("bucket_empty_lava") : TEXT("bucket_empty"), 1.f, 1.f);
			if (!I.SpawnMob.IsNone()) Game->SpawnMob(World, I.SpawnMob, FVector(T.X + 0.5, T.Y + 0.5, T.Z + 0.2), false);
		}
		if (!IsCreative()) ReplaceHeld(FMCItemStack::Of(TEXT("bucket"), 1), bOffhand);
		Swing();
		return true;
	}
	case EMCItemKind::Armor:
	case EMCItemKind::Elytra:
	{
		const EMCArmorSlot AS = I.Kind == EMCItemKind::Elytra ? EMCArmorSlot::Chest : I.ArmorSlot;
		const int32 Slot = MCInv::ArmorSlot(AS);
		if (Slot < 0) return false;
		Swap(Inventory.Slots[Slot], H);
		PlaySound(TEXT("armor_equip"), 1.f, 1.f);
		Swing();
		return true;
	}
	case EMCItemKind::Map:
		if (I.Name == TEXT("map"))
		{
			FMCItemStack M = FMCItemStack::Of(TEXT("filled_map"), 1);
			M.MutableExtra().MapId = Rand().NextInt(100000);
			ReplaceHeld(M, bOffhand);
			PlaySound(TEXT("map_create"), 1.f, 1.f);
			return true;
		}
		return false;
	default:
		break;
	}
	return false;
}
