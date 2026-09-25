// Living entities: health/armour/effects, Minecraft travel() physics, fall damage, drowning, death.
#include "Game/MCLiving.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCGame.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Blocks/MCTextures.h"

AMCLiving::AMCLiving()
{
	Gravity = 0.08;
	AirDrag = 0.98;
}

float AMCLiving::GetMaxHealth() const
{
	float M = MaxHealth;
	const int32 HB = EffectAmp(EMCEffect::HealthBoost);
	if (HB >= 0) M += 4.f * (HB + 1);
	return M;
}

void AMCLiving::Heal(float Amount)
{
	if (Health <= 0.f) return;
	Health = FMath::Min(GetMaxHealth(), Health + Amount);
}

float AMCLiving::GetSpeed() const
{
	float S = MoveSpeed;
	const int32 Sp = EffectAmp(EMCEffect::Speed);
	if (Sp >= 0) S *= 1.f + 0.2f * (Sp + 1);
	const int32 Sl = EffectAmp(EMCEffect::Slowness);
	if (Sl >= 0) S *= FMath::Max(0.f, 1.f - 0.15f * (Sl + 1));
	if (bSprinting) S *= 1.3f;
	return S;
}

bool AMCLiving::OnClimbable() const
{
	if (!World) return false;
	const FMCState S = World->GetState(BlockPos());
	const FMCStateInfo& I = FMCBlocks::Info(S);
	if (I.Flags & MCB_Climbable) return true;
	// open trapdoor above a ladder counts as climbable
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick

void AMCLiving::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	PrevBodyYaw = BodyYaw;
	PrevLimbSwingAmount = LimbSwingAmount;

	if (Health <= 0.f || DeathTime > 0)
	{
		TickDeath();
		// dead bodies still fall
		Vel.Z -= Gravity;
		Vel *= 0.91;
		Move(Vel);
		return;
	}

	if (HurtTime > 0) --HurtTime;
	if (InvulnerableTime > 0) --InvulnerableTime;
	if (NoJumpDelay > 0) --NoJumpDelay;

	TickEffects();
	if (bRemoved || Health <= 0.f) return;
	TickAir();
	TickFreezing();

	// suffocation inside opaque blocks
	if (!bNoPhysics)
	{
		const FVector Eye = GetEyePos();
		const FMCState ES = World->GetState(FMCBlockPos(MC::FloorToInt(Eye.X), MC::FloorToInt(Eye.Y), MC::FloorToInt(Eye.Z)));
		const FMCStateInfo& EI = FMCBlocks::Info(ES);
		if ((EI.Flags & MCB_Opaque) && EI.bFullCollision && !(EI.Flags & MCB_Fluid))
		{
			const AMCPlayer* P = Cast<AMCPlayer>(this);
			if (!(P && (P->IsCreative() || P->IsSpectator()))) Hurt(FMCDamage::Of(TEXT("in_wall")), 1.f);
		}
	}

	// arm swing progress (6 ticks, slower with mining fatigue)
	if (bSwinging)
	{
		++SwingTicks;
		int32 Dur = 6;
		const int32 Haste = EffectAmp(EMCEffect::Haste), Fatigue = EffectAmp(EMCEffect::MiningFatigue);
		if (Haste >= 0) Dur -= 1 + Haste;
		if (Fatigue >= 0) Dur += (1 + Fatigue) * 2;
		Dur = FMath::Max(Dur, 2);
		if (SwingTicks >= Dur) { bSwinging = false; SwingTicks = 0; }
		AttackAnim = (float)SwingTicks / Dur;
	}
	else AttackAnim = 0.f;

	if (AMCEntity* V = Vehicle.Get())
	{
		// passengers follow the vehicle
		if (V->bRemoved) StopRiding();
		else
		{
			Pos = V->Pos + V->GetPassengerOffset(this);
			Vel = FVector::ZeroVector;
			FallDistance = 0.f;
			bOnGround = false;
			AIStep();
			UpdateWalkAnimation();
			return;
		}
	}

	AIStep();
	UpdateWalkAnimation();

	// body yaw follows head yaw with a lag (Minecraft body rotation)
	const FVector HVel(Pos.X - PrevPos.X, Pos.Y - PrevPos.Y, 0.0);
	if (HVel.SizeSquared() > 0.0025)
	{
		const float MoveYaw = FMath::RadiansToDegrees(FMath::Atan2(HVel.Y, HVel.X));
		BodyYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(BodyYaw, MoveYaw), -30.f, 30.f) * 0.9f;
	}
	const float Delta = FMath::FindDeltaAngleDegrees(BodyYaw, Yaw);
	if (FMath::Abs(Delta) > 50.f) BodyYaw += Delta - FMath::Sign(Delta) * 50.f;
	HeadYaw = Yaw;
}

void AMCLiving::AIStep()
{
	if (bJumping)
	{
		if (bInWater || bInLava)
		{
			Vel.Z += 0.04;
		}
		else if (bOnGround && NoJumpDelay == 0)
		{
			Jump();
			NoJumpDelay = 10;
		}
	}
	else NoJumpDelay = 0;

	if (!IsPassenger()) Travel(MoveStrafe * 0.98f, MoveUp, MoveForward * 0.98f);

	// entity cramming / pushing
	if (IsPushable() && World && !IsPassenger())
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(GetBox().Inflate(0.2), Near, this);
		int32 Crammed = 0;
		for (AMCEntity* O : Near)
		{
			if (!O->IsPushable() || O->IsPassenger() || O->bRemoved) continue;
			PushAwayFrom(O);
			++Crammed;
		}
		if (Crammed > 24 && Age % 10 == 0) Hurt(FMCDamage::Of(TEXT("cramming")), 6.f);
	}
}

void AMCLiving::Jump()
{
	float Factor = 1.f;
	if (World)
	{
		const FMCState At = World->GetState(BlockPos());
		const FMCState Under = World->GetState(FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z - 0.5000001)));
		Factor = FMCBlocks::GetByState(At).JumpFactor;
		if (FMath::IsNearlyEqual(Factor, 1.f)) Factor = FMCBlocks::GetByState(Under).JumpFactor;
	}
	double J = JumpPower * Factor;
	const int32 JB = EffectAmp(EMCEffect::JumpBoost);
	if (JB >= 0) J += 0.1 * (JB + 1);
	Vel.Z = FMath::Max(Vel.Z, J);
	if (bSprinting)
	{
		const double R = FMath::DegreesToRadians((double)Yaw);
		Vel.X += FMath::Cos(R) * 0.2;
		Vel.Y += FMath::Sin(R) * 0.2;
	}
}

void AMCLiving::Travel(float Strafe, float Up, float Forward)
{
	if (!World) return;
	// input vector rotated by yaw (forward = +X at yaw 0, strafe positive = left)
	auto MoveRelative = [&](float Speed)
	{
		FVector2D In(Forward, -Strafe);
		const double L2 = In.SizeSquared();
		if (L2 < 1e-7) return;
		if (L2 > 1.0) In /= FMath::Sqrt(L2);
		In *= Speed;
		const double R = FMath::DegreesToRadians((double)Yaw);
		const double C = FMath::Cos(R), S = FMath::Sin(R);
		Vel.X += In.X * C - In.Y * S;
		Vel.Y += In.X * S + In.Y * C;
	};

	const bool bFalling = Vel.Z <= 0.0;
	double G = bNoGravity ? 0.0 : Gravity;
	if (bFalling && HasEffect(EMCEffect::SlowFalling)) { G = FMath::Min(G, 0.01); FallDistance = 0.f; }

	if (bInWater && !bElytraFlying)
	{
		double Drag = bSprinting ? 0.9 : 0.8;
		float Speed = 0.02f;
		const int32 Depth = GetEnchantMax(EMCEnchant::DepthStrider);
		if (Depth > 0)
		{
			const float F = FMath::Min(3, Depth) / 3.f * (bOnGround ? 1.f : 0.5f);
			Drag += (0.546 - Drag) * F;
			Speed += (GetSpeed() - Speed) * F;
		}
		if (HasEffect(EMCEffect::DolphinsGrace)) Drag = 0.96;
		if (bSprinting) Speed *= 1.6f;
		MoveRelative(Speed);
		Move(Vel);
		if (bHorizontalCollision && OnClimbable()) Vel.Z = 0.2;
		Vel.X *= Drag; Vel.Y *= Drag;
		Vel.Z *= 0.8;
		if (!bNoGravity) Vel.Z -= bSprinting && Forward > 0 ? 0.005 : 0.02 / 4.0;
		// climb out of water onto a ledge
		if (bHorizontalCollision && World->IsRegionFree(GetBox().Offset(Vel.X, Vel.Y, Vel.Z + 0.6 - Pos.Z + PrevPos.Z)))
		{
			Vel.Z = 0.3;
		}
		return;
	}
	if (bInLava && !bElytraFlying)
	{
		MoveRelative(0.02f);
		Move(Vel);
		Vel *= 0.5;
		if (!bNoGravity) Vel.Z -= G / 4.0;
		if (bHorizontalCollision && World->IsRegionFree(GetBox().Offset(Vel.X, Vel.Y, Vel.Z + 0.6 - Pos.Z + PrevPos.Z))) Vel.Z = 0.3;
		return;
	}
	if (bElytraFlying)
	{
		// Minecraft elytra glide
		const FVector Look = GetLookDir();
		const double PitchR = FMath::DegreesToRadians((double)Pitch);
		const double HLook = FMath::Sqrt(Look.X * Look.X + Look.Y * Look.Y);
		const double HVel = FMath::Sqrt(Vel.X * Vel.X + Vel.Y * Vel.Y);
		double Lift = FMath::Cos(PitchR);
		Lift = Lift * Lift * FMath::Min(1.0, Look.Size() / 0.4);
		Vel.Z += G * (-1.0 + Lift * 0.75);
		if (Vel.Z < 0.0 && HLook > 0.0)
		{
			const double D = Vel.Z * -0.1 * Lift;
			Vel.Z += D;
			Vel.X += Look.X * D / HLook;
			Vel.Y += Look.Y * D / HLook;
		}
		if (PitchR > 0.0 && HLook > 0.0)
		{
			const double D = HVel * FMath::Sin(PitchR) * 0.04;
			Vel.Z += D * 3.2;
			Vel.X -= Look.X * D / HLook;
			Vel.Y -= Look.Y * D / HLook;
		}
		if (HLook > 0.0)
		{
			Vel.X += (Look.X / HLook * HVel - Vel.X) * 0.1;
			Vel.Y += (Look.Y / HLook * HVel - Vel.Y) * 0.1;
		}
		Vel *= FVector(0.99, 0.99, 0.98);
		const double PreSpeed = Vel.Size();
		Move(Vel);
		if (bHorizontalCollision)
		{
			const double Impact = (PreSpeed - Vel.Size()) * 10.0 - 3.0;
			if (Impact > 0.0) Hurt(FMCDamage::Of(TEXT("fly_into_wall")), (float)Impact);
		}
		if (bOnGround) bElytraFlying = false;
		return;
	}

	// ground / air
	const FMCState Under = World->GetState(FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z - 0.5000001)));
	const float Slip = bOnGround ? FMCBlocks::GetByState(Under).Friction : 1.f;
	const float Friction = Slip * 0.91f;
	const float Speed = bOnGround ? GetSpeed() * (0.21600002f / (Slip * Slip * Slip)) : FlyingSpeed;
	MoveRelative(Speed * BlockSpeedFactor);
	const bool bClimbing = OnClimbable();
	if (bClimbing)
	{
		Vel.X = FMath::Clamp(Vel.X, -0.15, 0.15);
		Vel.Y = FMath::Clamp(Vel.Y, -0.15, 0.15);
		Vel.Z = FMath::Max(Vel.Z, -0.15);
		if (Vel.Z < 0.0 && bSneaking && Kind == EMCEntityKind::Player) Vel.Z = 0.0;
		FallDistance = 0.f;
	}
	Move(Vel, PreventsEdgeFall());
	if (bClimbing && (bHorizontalCollision || bJumping)) Vel.Z = 0.2;

	const int32 Lev = EffectAmp(EMCEffect::Levitation);
	if (Lev >= 0)
	{
		Vel.Z += (0.05 * (Lev + 1) - Vel.Z) * 0.2;
		FallDistance = 0.f;
	}
	else
	{
		Vel.Z -= G;
	}
	Vel.Z *= 0.98;
	Vel.X *= Friction;
	Vel.Y *= Friction;
}

// ---------------------------------------------------------------------------------------------------------------------
// Damage

int32 AMCLiving::GetArmorValue() const
{
	float A = BaseArmor;
	for (int32 s = (int32)EMCEquipSlot::Head; s <= (int32)EMCEquipSlot::Feet; ++s)
	{
		const FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (!S.IsEmpty()) A += S.Item().ArmorPoints;
	}
	return FMath::Min(30, FMath::RoundToInt(A));
}

float AMCLiving::GetArmorToughness() const
{
	float T = 0.f;
	for (int32 s = (int32)EMCEquipSlot::Head; s <= (int32)EMCEquipSlot::Feet; ++s)
	{
		const FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (!S.IsEmpty()) T += S.Item().Toughness;
	}
	return T;
}

int32 AMCLiving::GetProtectionLevel(const FMCDamage& D) const
{
	if (D.bBypassInvulnerability) return 0;
	int32 EPF = 0;
	for (int32 s = (int32)EMCEquipSlot::Head; s <= (int32)EMCEquipSlot::Feet; ++s)
	{
		const FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (S.IsEmpty()) continue;
		EPF += S.GetEnchant(EMCEnchant::Protection);
		if (D.bFire) EPF += 2 * S.GetEnchant(EMCEnchant::FireProtection);
		if (D.bExplosion) EPF += 2 * S.GetEnchant(EMCEnchant::BlastProtection);
		if (D.bProjectile) EPF += 2 * S.GetEnchant(EMCEnchant::ProjectileProtection);
		if (D.bFall) EPF += 3 * S.GetEnchant(EMCEnchant::FeatherFalling);
	}
	return FMath::Min(20, EPF);
}

float AMCLiving::ApplyArmor(const FMCDamage& D, float Amount)
{
	if (D.bBypassArmor) return Amount;
	DamageArmor(Amount);
	const float Armor = (float)GetArmorValue();
	const float Tough = GetArmorToughness();
	const float F = FMath::Clamp(Armor - Amount / (2.f + Tough / 4.f), Armor * 0.2f, 20.f);
	return Amount * (1.f - F / 25.f);
}

float AMCLiving::ApplyMagicReduction(const FMCDamage& D, float Amount)
{
	const int32 Res = EffectAmp(EMCEffect::Resistance);
	if (Res >= 0 && D.Type != TEXT("out_of_world") && D.Type != TEXT("generic_kill"))
	{
		Amount *= FMath::Max(0.f, 1.f - 0.2f * (Res + 1));
	}
	if (Amount <= 0.f) return 0.f;
	const int32 EPF = GetProtectionLevel(D);
	if (EPF > 0) Amount *= 1.f - EPF / 25.f;
	return FMath::Max(0.f, Amount);
}

void AMCLiving::DamageArmor(float Amount)
{
	if (Amount <= 0.f) return;
	const int32 Dmg = FMath::Max(1, FMath::FloorToInt(Amount / 4.f));
	for (int32 s = (int32)EMCEquipSlot::Head; s <= (int32)EMCEquipSlot::Feet; ++s)
	{
		FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (S.IsEmpty() || !S.IsDamageable()) continue;
		if (S.DamageItem(Dmg, Rand())) PlaySound(TEXT("item_break"), 0.8f, 0.9f);
	}
}

void AMCLiving::Knockback(double Strength, double DirX, double DirY)
{
	Strength *= 1.0 - KnockbackResistance;
	if (Strength <= 0.0) return;
	FVector2D D(DirX, DirY);
	if (D.SizeSquared() < 1e-6) D = FVector2D(Rand().FRange(-1.f, 1.f), Rand().FRange(-1.f, 1.f));
	D.Normalize();
	D *= Strength;
	Vel.X = Vel.X / 2.0 - D.X;
	Vel.Y = Vel.Y / 2.0 - D.Y;
	if (bOnGround) Vel.Z = FMath::Min(0.4, Vel.Z / 2.0 + Strength);
}

bool AMCLiving::Hurt(const FMCDamage& D, float Amount)
{
	if (!IsAlive() || !World) return false;
	if (bInvulnerable && !D.bBypassInvulnerability) return false;
	if (D.bFire && (bFireImmune || HasEffect(EMCEffect::FireResistance))) return false;
	if (D.Type == TEXT("drown") && CanBreatheUnderwater()) return false;
	if (Game && D.bFall && !Game->Rules.bFallDamage && Kind == EMCEntityKind::Player) return false;

	// invulnerability frames: only a stronger hit gets through (difference)
	bool bFullHit = true;
	if (InvulnerableTime > 10 && !D.bBypassInvulnerability)
	{
		if (Amount <= LastHurtAmount) return false;
		const float Extra = Amount - LastHurtAmount;
		LastHurtAmount = Amount;
		Amount = Extra;
		bFullHit = false;
	}
	else
	{
		LastHurtAmount = Amount;
		InvulnerableTime = 20;
		HurtTime = 10;
	}

	float Final = ApplyArmor(D, Amount);
	Final = ApplyMagicReduction(D, Final);
	if (Absorption > 0.f)
	{
		const float Abs = FMath::Min(Absorption, Final);
		Absorption -= Abs;
		Final -= Abs;
	}
	if (D.Attacker.IsValid())
	{
		LastAttacker = D.Attacker;
		LastAttackerTime = Age;
		if (AMCLiving* Att = Cast<AMCLiving>(D.Attacker.Get())) Att->LastHurtMob = this;
	}
	if (bFullHit && !D.bNoKnockback && (D.bHasSourcePos || D.Attacker.IsValid()))
	{
		const FVector Src = D.bHasSourcePos ? D.SourcePos : D.Attacker->Pos;
		Knockback(D.KnockbackStrength, Src.X - Pos.X, Src.Y - Pos.Y);
	}
	// thorns
	if (D.Attacker.IsValid() && !D.bProjectile && D.Type != TEXT("thorns"))
	{
		const int32 Thorns = GetEnchantMax(EMCEnchant::Thorns);
		if (Thorns > 0 && Rand().Chance(0.15 * Thorns))
		{
			FMCDamage TD = FMCDamage::Of(TEXT("thorns"));
			TD.Attacker = this; TD.Direct = this; TD.bNoKnockback = false; TD.SourcePos = Pos; TD.bHasSourcePos = true;
			D.Attacker->Hurt(TD, (float)Rand().Range(1, 4));
		}
	}

	if (Final > 0.f) Health -= Final;
	OnHurtEffect(D);
	if (Health <= 0.f)
	{
		// totem of undying in either hand
		for (EMCEquipSlot Hand : { EMCEquipSlot::MainHand, EMCEquipSlot::OffHand })
		{
			FMCItemStack& S = GetItem(Hand);
			if (!S.IsEmpty() && S.Item().Name == TEXT("totem_of_undying") && !D.bBypassInvulnerability)
			{
				S.Count -= 1;
				if (S.Count <= 0) S.Clear();
				Health = 1.f;
				ClearEffects();
				FMCEffectInstance E; E.Effect = EMCEffect::Regeneration; E.Duration = 900; E.Amplifier = 1; AddEffect(E);
				E.Effect = EMCEffect::Absorption; E.Duration = 100; E.Amplifier = 1; AddEffect(E);
				E.Effect = EMCEffect::FireResistance; E.Duration = 800; E.Amplifier = 0; AddEffect(E);
				PlaySound(TEXT("totem_use"), 1.f, 1.f);
				World->SpawnParticles(TEXT("totem"), Pos + FVector(0, 0, Height * 0.5), 60, 0.8f, FVector(0, 0, 0.4), FColor(250, 220, 60));
				return true;
			}
		}
		Health = 0.f;
		Die(D);
	}
	return true;
}

void AMCLiving::Die(const FMCDamage& D)
{
	if (DeathTime > 0) return;
	DeathTime = 1;
	Health = 0.f;
	bJumping = false;
	MoveForward = MoveStrafe = 0.f;
	LastDeathMessage = D.DeathMessage(GetDisplayName());
	AMCEntity* Killer = D.Attacker.Get();
	if (!Killer && LastAttacker.IsValid() && Age - LastAttackerTime < 100) Killer = LastAttacker.Get();
	const bool bPlayerKill = Killer && Killer->IsA<AMCPlayer>();
	if (!Game || Game->Rules.bDoMobLoot) DropLoot(D, bPlayerKill);
	if (bPlayerKill || (Killer && Killer->IsA<AMCMob>() && Cast<AMCMob>(Killer)->bTamed))
	{
		const int32 XP = GetXPReward();
		if (XP > 0 && World) World->SpawnXP(Pos + FVector(0, 0, 0.3), XP);
		if (AMCPlayer* P = Cast<AMCPlayer>(Killer)) ++P->StatMobsKilled;
	}
	StopRiding();
}

void AMCLiving::TickDeath()
{
	++DeathTime;
	if (DeathTime >= 20)
	{
		OnDeathAnimationFinished();
	}
}

void AMCLiving::OnDeathAnimationFinished()
{
	if (World) World->SpawnParticles(TEXT("poof"), Pos + FVector(0, 0, Height * 0.5), 20, Width * 0.6f, FVector(0, 0, 0.05), FColor(230, 230, 230));
	Discard();
}

void AMCLiving::OnFellOnGround(float Distance, FMCState Landed)
{
	if (!World || bInWater) return;
	float Mult = Landed ? FMCBlocks::GetByState(Landed).Behavior->FallDamageMultiplier(Landed) : 1.f;
	int32 JB = EffectAmp(EMCEffect::JumpBoost);
	const float Safe = 3.f + (JB >= 0 ? JB + 1 : 0);
	const int32 Dmg = FMath::CeilToInt((Distance - Safe) * Mult);
	if (Dmg > 0)
	{
		PlaySound(Dmg > 4 ? TEXT("fall_big") : TEXT("fall_small"), 1.f, 1.f);
		Hurt(FMCDamage::Of(TEXT("fall")), (float)Dmg);
		if (Landed && Distance > 3.f)
		{
			World->SpawnParticles(TEXT("block_dust"), Pos, FMath::Min(40, (int32)(Distance * 3)), Width * 0.5f, FVector(0, 0, 0.1), FMCTextures::AverageColor(FMCBlocks::Info(Landed).Tex[1]));
		}
	}
}

void AMCLiving::CheckFallDamage(double DeltaZ, bool bGroundNow)
{
	if (HasEffect(EMCEffect::SlowFalling) || HasEffect(EMCEffect::Levitation) || bElytraFlying) { FallDistance = 0.f; if (!bElytraFlying) { Super::CheckFallDamage(0.0, bGroundNow); } return; }
	if (OnClimbable()) FallDistance = 0.f;
	Super::CheckFallDamage(DeltaZ, bGroundNow);
}

void AMCLiving::TickAir()
{
	if (bEyesInWater && !CanBreatheUnderwater())
	{
		const int32 Resp = GetEnchantMax(EMCEnchant::Respiration);
		const AMCPlayer* P = Cast<AMCPlayer>(this);
		if (!(P && (P->IsCreative() || P->IsSpectator())) && !(Resp > 0 && Rand().NextInt(Resp + 1) > 0)) --AirSupply;
		if (AirSupply <= -20)
		{
			AirSupply = 0;
			if (!Game || Game->Rules.bDrowningDamage) Hurt(FMCDamage::Of(TEXT("drown")), 2.f);
			if (World) World->SpawnParticles(TEXT("bubble"), GetEyePos(), 8, 0.3f, FVector(0, 0, 0.2), FColor(200, 220, 255));
		}
	}
	else if (AirSupply < MaxAir)
	{
		AirSupply = FMath::Min(MaxAir, AirSupply + 4);
	}
}

void AMCLiving::TickFreezing()
{
	if (FreezeTicks >= 140 && Age % 40 == 0)
	{
		const AMCPlayer* P = Cast<AMCPlayer>(this);
		if (!(P && P->IsCreative())) Hurt(FMCDamage::Of(TEXT("freeze")), IsA<AMCMob>() && Cast<AMCMob>(this)->Def && Cast<AMCMob>(this)->Def->Id == TEXT("strider") ? 5.f : 1.f);
	}
}

void AMCLiving::UpdateWalkAnimation()
{
	const double DX = Pos.X - PrevPos.X, DY = Pos.Y - PrevPos.Y;
	double Dist = FMath::Sqrt(DX * DX + DY * DY) * 4.0;
	if (Dist > 1.0) Dist = 1.0;
	LimbSwingAmount += ((float)Dist - LimbSwingAmount) * 0.4f;
	LimbSwing += LimbSwingAmount;
}

void AMCLiving::Swing()
{
	if (!bSwinging || SwingTicks >= 3 || SwingTicks < 0)
	{
		SwingTicks = -1;
		bSwinging = true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Effects

bool AMCLiving::HasEffect(EMCEffect E) const { return GetEffect(E) != nullptr; }

const FMCEffectInstance* AMCLiving::GetEffect(EMCEffect E) const
{
	for (const FMCEffectInstance& I : Effects) if (I.Effect == E && I.Duration > 0) return &I;
	return nullptr;
}

bool AMCLiving::AddEffect(const FMCEffectInstance& E)
{
	if (!IsAffectedByPotions() || E.Effect == EMCEffect::None) return false;
	if (MCEffects::IsInstant(E.Effect))
	{
		ApplyInstantEffect(E.Effect, E.Amplifier, nullptr);
		return true;
	}
	for (FMCEffectInstance& I : Effects)
	{
		if (I.Effect != E.Effect) continue;
		if (E.Amplifier > I.Amplifier || (E.Amplifier == I.Amplifier && E.Duration > I.Duration))
		{
			const uint8 OldAmp = I.Amplifier;
			I = E;
			if (E.Effect == EMCEffect::Absorption && E.Amplifier >= OldAmp) Absorption = FMath::Max(Absorption, 4.f * (E.Amplifier + 1));
		}
		return true;
	}
	Effects.Add(E);
	if (E.Effect == EMCEffect::Absorption) Absorption = FMath::Max(Absorption, 4.f * (E.Amplifier + 1));
	if (E.Effect == EMCEffect::Glowing) bGlowing = true;
	if (E.Effect == EMCEffect::HealthBoost) {}
	return true;
}

void AMCLiving::RemoveEffect(EMCEffect E)
{
	Effects.RemoveAll([E](const FMCEffectInstance& I) { return I.Effect == E; });
	if (E == EMCEffect::Glowing) bGlowing = false;
	if (E == EMCEffect::Absorption) Absorption = 0.f;
	if (E == EMCEffect::HealthBoost) Health = FMath::Min(Health, GetMaxHealth());
}

void AMCLiving::ClearEffects()
{
	Effects.Reset();
	bGlowing = false;
	Absorption = 0.f;
	Health = FMath::Min(Health, GetMaxHealth());
}

void AMCLiving::ApplyInstantEffect(EMCEffect E, int32 Amplifier, AMCEntity* Source)
{
	const bool bHealing = E == EMCEffect::InstantHealth;
	const bool bInvert = bUndead;
	if (bHealing != bInvert)
	{
		Heal((float)(4 << FMath::Min(Amplifier, 8)));
	}
	else
	{
		FMCDamage D = FMCDamage::Of(Source ? TEXT("indirect_magic") : TEXT("magic"));
		D.Attacker = Source;
		Hurt(D, (float)(6 << FMath::Min(Amplifier, 8)));
	}
}

void AMCLiving::TickEffects()
{
	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		FMCEffectInstance& E = Effects[i];
		const int32 Amp = E.Amplifier;
		switch (E.Effect)
		{
		case EMCEffect::Regeneration:
		{
			const int32 Int = FMath::Max(1, 50 >> FMath::Min(Amp, 5));
			if (E.Duration % Int == 0 && Health < GetMaxHealth()) Heal(1.f);
			break;
		}
		case EMCEffect::Poison:
		{
			const int32 Int = FMath::Max(1, 25 >> FMath::Min(Amp, 5));
			if (E.Duration % Int == 0 && Health > 1.f && !bUndead) Hurt(FMCDamage::Of(TEXT("magic")), FMath::Min(1.f, Health - 1.f));
			break;
		}
		case EMCEffect::Wither:
		{
			const int32 Int = FMath::Max(1, 40 >> FMath::Min(Amp, 5));
			if (E.Duration % Int == 0) Hurt(FMCDamage::Of(TEXT("wither")), 1.f);
			break;
		}
		case EMCEffect::Hunger:
			if (AMCPlayer* P = Cast<AMCPlayer>(this)) P->CauseExhaustion(0.005f * (Amp + 1));
			break;
		case EMCEffect::Saturation:
			if (AMCPlayer* P = Cast<AMCPlayer>(this))
			{
				P->FoodLevel = FMath::Min(20, P->FoodLevel + Amp + 1);
				P->Saturation = FMath::Min((float)P->FoodLevel, P->Saturation + 2.f * (Amp + 1));
			}
			break;
		case EMCEffect::InstantHealth:
		case EMCEffect::InstantDamage:
			ApplyInstantEffect(E.Effect, Amp, nullptr);
			E.Duration = 0;
			break;
		default: break;
		}
		if (bRemoved || !Effects.IsValidIndex(i)) return;
		if (--Effects[i].Duration <= 0)
		{
			const EMCEffect Gone = Effects[i].Effect;
			Effects.RemoveAt(i);
			if (Gone == EMCEffect::Glowing) bGlowing = false;
			if (Gone == EMCEffect::Absorption) Absorption = 0.f;
			if (Gone == EMCEffect::HealthBoost) Health = FMath::Min(Health, GetMaxHealth());
		}
	}
	// ambient effect particles
	if (Effects.Num() > 0 && World && Age % 5 == 0)
	{
		const FMCEffectInstance& E = Effects[Rand().NextInt(Effects.Num())];
		if (E.bShowParticles && !HasEffect(EMCEffect::Invisibility))
		{
			World->SpawnParticles(TEXT("effect"), Pos + FVector(Rand().FRange(-Width, Width) * 0.5, Rand().FRange(-Width, Width) * 0.5, Rand().FRange(0.f, Height)), 1, 0.f, FVector(0, 0, 0.05), MCEffects::Color(E.Effect));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Equipment helpers

void AMCLiving::BreakItem(EMCEquipSlot S)
{
	FMCItemStack& I = GetItem(S);
	if (I.IsEmpty()) return;
	PlaySound(TEXT("item_break"), 0.8f, 0.8f + Rand().NextFloat() * 0.4f);
	I.Clear();
}

int32 AMCLiving::GetEnchantTotal(EMCEnchant E) const
{
	int32 T = 0;
	for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s)
	{
		const FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (!S.IsEmpty()) T += S.GetEnchant(E);
	}
	return T;
}

int32 AMCLiving::GetEnchantMax(EMCEnchant E) const
{
	int32 M = 0;
	for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s)
	{
		const FMCItemStack& S = GetItem((EMCEquipSlot)s);
		if (!S.IsEmpty()) M = FMath::Max(M, S.GetEnchant(E));
	}
	return M;
}

bool AMCLiving::CanSee(const AMCEntity* Other) const
{
	if (!Other || !World) return false;
	const FVector From = GetEyePos();
	const FVector To = Other->GetEyePos();
	FVector Dir = To - From;
	const double Len = Dir.Size();
	if (Len < 1e-3) return true;
	if (Len > 128.0) return false;
	Dir /= Len;
	FMCRayHit Hit;
	return !World->Raycast(From, Dir, Len, Hit, false, false);
}

void AMCLiving::LookAt(const FVector& Target, float MaxYawStep, float MaxPitchStep)
{
	const FVector D = Target - GetEyePos();
	const float TYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const float TPitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, FMath::Sqrt(D.X * D.X + D.Y * D.Y)));
	Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, TYaw), -MaxYawStep, MaxYawStep);
	Pitch += FMath::Clamp(TPitch - Pitch, -MaxPitchStep, MaxPitchStep);
	Pitch = FMath::Clamp(Pitch, -90.f, 90.f);
}

void AMCLiving::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	int32 Version = 1;
	Ar << Version;
	Ar << Health << Absorption << AirSupply << bSprinting;
	Ar << Effects;
	for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s)
	{
		if (Kind == EMCEntityKind::Player) { FMCItemStack Dummy; Ar << Dummy; }
		else Ar << Equipment[s];
	}
	for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s) Ar << DropChances[s];
}
