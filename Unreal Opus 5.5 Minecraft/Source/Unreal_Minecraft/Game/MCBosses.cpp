// Bosses: the Ender Dragon (phases, crystals, breath, perching, death sequence) and the Wither.
#include "Game/MCMob.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCEntities.h"
#include "Gen/MCEndGen.h"
#include "Render/MCRig.h"
#include "World/MCWorld.h"

namespace
{
	bool IsDragonImmune(const FMCBlock& B)
	{
		static const TCHAR* Names[] = { TEXT("bedrock"), TEXT("end_portal"), TEXT("end_portal_frame"), TEXT("end_gateway"), TEXT("obsidian"), TEXT("crying_obsidian"),
			TEXT("end_stone"), TEXT("iron_bars"), TEXT("respawn_anchor"), TEXT("reinforced_deepslate"), TEXT("barrier") };
		for (const TCHAR* N : Names) if (B.Name == FName(N)) return true;
		return B.Hardness < 0.f || B.Has(MCB_Unbreakable);
	}

	bool IsWitherImmune(const FMCBlock& B)
	{
		static const TCHAR* Names[] = { TEXT("bedrock"), TEXT("end_portal"), TEXT("end_portal_frame"), TEXT("end_gateway"), TEXT("reinforced_deepslate"), TEXT("barrier") };
		for (const TCHAR* N : Names) if (B.Name == FName(N)) return true;
		return B.Hardness < 0.f || B.Has(MCB_Unbreakable);
	}
}

// =====================================================================================================================
// Ender Dragon

AMCEnderDragon::AMCEnderDragon()
{
	bPersistent = true;
}

FVector AMCEnderDragon::PortalCenter() const
{
	int32 Z = 64;
	if (World && World->Generator)
	{
		if (const FMCEndGen* G = static_cast<const FMCEndGen*>(World->Generator.Get())) Z = G->ExitPortalZ();
	}
	return FVector(0.5, 0.5, Z + 1);
}

void AMCEnderDragon::InitEntity()
{
	if (!Def) SetDefinition(MCMobs::Find(TEXT("ender_dragon")), 0);
	Super::InitEntity();
	bNoGravity = true;
	bNoPhysics = true;       // the dragon flies through (and destroys) terrain
	Gravity = 0.0;
	Phase = EPhase::Circling;
	CircleIndex = Rand().NextInt(12);
	Waypoint = Pos;
	PosHistory.Init(Pos, 64);
}

void AMCEnderDragon::SetPhase(EPhase P)
{
	Phase = P;
	PhaseTime = 0;
	DamageSinceStateChange = 0.f;
	if (P == EPhase::Circling) bClockwise = Rand().NextInt(8) != 0 ? bClockwise : !bClockwise;
}

void AMCEnderDragon::TickEntity()
{
	PrevWingFlap = WingFlap;
	Super::TickEntity();
	PosHistory.Insert(Pos, 0);
	if (PosHistory.Num() > 64) PosHistory.SetNum(64);
}

void AMCEnderDragon::AIStep()
{
	if (!World || !Game) return;
	if (Phase == EPhase::Dying) { TickDeathSequence(); return; }
	++PhaseTime;
	AMCPlayer* P = Game->Player && Game->Player->World == World && Game->Player->IsAlive() && !Game->Player->IsCreative() && !Game->Player->IsSpectator() ? Game->Player : nullptr;
	const FVector Center = PortalCenter();
	// wing flap speed depends on the phase
	const bool bPerching = Phase == EPhase::Perching || Phase == EPhase::Breathing;
	WingFlap += bPerching ? 0.1f : (Phase == EPhase::Charging ? 0.3f : 0.2f);
	if (FMath::Fmod(WingFlap, 2.f * PI) < FMath::Fmod(PrevWingFlap, 2.f * PI) && !bPerching) PlaySound(TEXT("ender_dragon_flap"), 5.f, 0.8f + Rand().NextFloat() * 0.3f);

	float Speed = 0.6f;
	switch (Phase)
	{
	case EPhase::Circling:
	{
		// fly the ring of pillar nodes
		const float Radius = 60.f;
		const float A = (CircleIndex % 12) * (2.f * PI / 12.f);
		Waypoint = FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, Center.Z + 20.f + 10.f * FMath::Sin(A * 3.f));
		if (FVector::DistSquared(Pos, Waypoint) < 100.0)
		{
			CircleIndex += bClockwise ? 1 : 11;
			// decide what to do next
			const int32 Crystals = Game->CountEndCrystals();
			if (P && Rand().NextInt(Crystals + 3) == 0) SetPhase(EPhase::Approaching);
			else if (P && Rand().NextInt(2 + Crystals) == 0 && FVector::DistSquared(P->Pos, Pos) < 150.0 * 150.0) SetPhase(EPhase::Strafing);
		}
		break;
	}
	case EPhase::Strafing:
	{
		if (!P) { SetPhase(EPhase::Circling); break; }
		Waypoint = P->Pos + FVector(0, 0, 8);
		const double D = FVector::Dist(P->Pos, Pos);
		if (D < 64.0 && CanSee(P) && PhaseTime > 20)
		{
			// dragon fireball
			if (AMCProjectile* F = Game->SpawnEntity<AMCProjectile>(World, Pos + GetLookDir() * 6.0 + FVector(0, 0, 2)))
			{
				F->Type = EMCProjectile::DragonFireball;
				F->Shooter = this;
				F->bPickup = false;
				F->InitEntity();
				const FVector Dir = (P->Pos - F->Pos).GetSafeNormal();
				F->Shoot(Dir, 1.2f, 0.f);
				F->Accel = Dir * 0.1;
			}
			PlaySound(TEXT("ender_dragon_shoot"), 5.f, 1.f);
			SetPhase(EPhase::Circling);
		}
		if (PhaseTime > 200) SetPhase(EPhase::Circling);
		break;
	}
	case EPhase::Approaching:
		Waypoint = Center + FVector(0, 0, 12);
		Speed = 0.5f;
		if (FVector::DistSquared(Pos, Waypoint) < 36.0) SetPhase(EPhase::Landing);
		break;
	case EPhase::Landing:
		Waypoint = Center + FVector(0, 0, 0.5);
		Speed = 0.25f;
		if (FVector::DistSquared(Pos, Waypoint) < 2.0) { SetPhase(EPhase::Perching); PlaySound(TEXT("ender_dragon_growl"), 5.f, 1.f); }
		break;
	case EPhase::Perching:
		Vel = FVector::ZeroVector;
		if (P) LookAt(P->GetEyePos(), 5.f, 5.f);
		if (PhaseTime > 25 && P && FVector::DistSquared(P->Pos, Pos) < 400.0 && Rand().NextInt(3) == 0) SetPhase(EPhase::Breathing);
		if (PhaseTime > 100 || DamageSinceStateChange > 20.f) SetPhase(Rand().NextInt(2) == 0 && P ? EPhase::Charging : EPhase::TakingOff);
		break;
	case EPhase::Breathing:
	{
		Vel = FVector::ZeroVector;
		if (PhaseTime == 1) PlaySound(TEXT("ender_dragon_growl"), 5.f, 1.f);
		if (PhaseTime % 5 == 0)
		{
			// breath cloud in front of the head
			const FVector Head = Pos + FRotator(0, Yaw, 0).Vector() * 8.0 + FVector(0, 0, 1);
			if (AMCAreaCloud* C = Game->SpawnEntity<AMCAreaCloud>(World, FVector(Head.X, Head.Y, Center.Z)))
			{
				C->bDragonBreath = true; C->Effect = EMCEffect::InstantDamage; C->Radius = 5.f; C->Duration = 200; C->Color = FColor(180, 40, 220); C->Owner = this;
			}
			World->SpawnParticles(TEXT("dragon_breath"), Head, 30, 2.5f, FVector(0, 0, -0.2), FColor(200, 60, 230));
		}
		if (PhaseTime > 100) SetPhase(EPhase::TakingOff);
		break;
	}
	case EPhase::TakingOff:
		Waypoint = Center + FVector(0, 0, 25) + FRotator(0, Yaw, 0).Vector() * 30.0;
		Speed = 0.5f;
		if (Pos.Z > Center.Z + 20.0 || PhaseTime > 120) SetPhase(EPhase::Circling);
		break;
	case EPhase::Charging:
		if (!P) { SetPhase(EPhase::TakingOff); break; }
		Waypoint = P->GetEyePos();
		Speed = 1.0f;
		if (FVector::DistSquared(Pos, Waypoint) < 25.0 || PhaseTime > 100) SetPhase(EPhase::TakingOff);
		break;
	default: break;
	}

	// steering (MC dragon turns gradually and flies forward)
	if (Phase != EPhase::Perching && Phase != EPhase::Breathing)
	{
		FVector D = Waypoint - Pos;
		const double L = D.Size();
		if (L > 0.1)
		{
			D /= L;
			const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, TargetYaw), -8.f, 8.f);
			const FVector Fwd = FRotator(0, Yaw, 0).Vector();
			const double Align = FMath::Max(0.25, FVector::DotProduct(FVector(D.X, D.Y, 0).GetSafeNormal(), Fwd));
			Vel += (Fwd * Speed * Align + FVector(0, 0, FMath::Clamp(D.Z, -0.6, 0.6) * Speed) - Vel) * 0.08;
			Pitch = FMath::Lerp(Pitch, (float)FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(D.Z, -1.0, 1.0))) * 0.5f, 0.1f);
		}
	}
	Pos += Vel;
	BodyYaw = Yaw;
	HeadYaw = Yaw;

	TickCrystalHealing();
	DestroyBlocksInBody();

	// collisions: wings knock back, head hurts
	TArray<AMCEntity*> Near;
	TArray<FMCBox> Parts;
	GetPartBoxes(Parts);
	if (Parts.Num() > 0)
	{
		FMCBox All = Parts[0];
		for (const FMCBox& B : Parts) { All.Min = All.Min.ComponentMin(B.Min); All.Max = All.Max.ComponentMax(B.Max); }
		World->GetEntitiesInBox(All.Inflate(1.0), Near, this);
		for (AMCEntity* E : Near)
		{
			AMCLiving* L = Cast<AMCLiving>(E);
			if (!L || L == this || !L->IsAlive() || L->IsA<AMCEnderDragon>()) continue;
			const FMCBox EB = L->GetBox();
			if (Parts[0].Intersects(EB) && Phase != EPhase::Perching && Phase != EPhase::Breathing)
			{
				// head / body hit
				L->Hurt(FMCDamage::Mob(this), 10.f);
			}
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				if (!Parts[i].Intersects(EB)) continue;
				const FVector Away = (L->Pos - Pos).GetSafeNormal2D();
				L->Vel += Away * 2.0 + FVector(0, 0, 0.3);
				if (Phase != EPhase::Perching) L->Hurt(FMCDamage::Mob(this), 5.f);
				break;
			}
		}
	}
	if (Age % 200 == 0 && Rand().NextInt(3) == 0) PlaySound(TEXT("ender_dragon_ambient"), 5.f, 0.8f + Rand().NextFloat() * 0.3f);
}

void AMCEnderDragon::GetPartBoxes(TArray<FMCBox>& Out) const
{
	Out.Reset();
	const FVector Fwd = FRotator(0, Yaw, 0).Vector();
	const FVector Right(-Fwd.Y, Fwd.X, 0);
	const FVector Body = Pos + FVector(0, 0, 3.0);
	auto Box = [&](const FVector& C, double HX, double HZ) { Out.Add(FMCBox(C - FVector(HX, HX, HZ), C + FVector(HX, HX, HZ))); };
	Box(Body + Fwd * 7.5 + FVector(0, 0, 1.0), 1.0, 1.0);          // head (index 0)
	Box(Body, 2.5, 1.5);                                           // body
	Box(Body + Fwd * 4.5 + FVector(0, 0, 0.5), 1.5, 1.0);          // neck
	const float Flap = FMath::Sin(WingFlap) * 1.5f;
	Box(Body + Right * 5.0 + FVector(0, 0, Flap), 2.0, 0.5);       // right wing
	Box(Body - Right * 5.0 + FVector(0, 0, Flap), 2.0, 0.5);       // left wing
	for (int32 i = 1; i <= 3; ++i)
	{
		const int32 H = FMath::Clamp(i * 5, 0, PosHistory.Num() - 1);
		const FVector T = PosHistory.IsValidIndex(H) ? PosHistory[H] : Pos;
		Box(T + FVector(0, 0, 3.0) - Fwd * (3.0 + i * 1.5), 1.0, 1.0);
	}
}

bool AMCEnderDragon::RayHitParts(const FVector& Origin, const FVector& Dir, double MaxDist, double& OutT) const
{
	TArray<FMCBox> Parts;
	GetPartBoxes(Parts);
	bool bHit = false;
	OutT = MaxDist;
	for (const FMCBox& B : Parts)
	{
		double T; EMCFace F;
		if (B.RayHit(Origin, Dir, MaxDist, T, F) && T < OutT) { OutT = T; bHit = true; }
	}
	return bHit;
}

bool AMCEnderDragon::Hurt(const FMCDamage& D, float Amount)
{
	if (Phase == EPhase::Dying || !IsAlive()) return false;
	if (D.Attacker.Get() == this) return false;
	if (D.bExplosion && D.Direct.IsValid() && D.Direct->IsA<AMCEndCrystal>() == false && D.Direct.Get() == this) return false;
	// arrows bounce off while perched (Minecraft)
	if (D.bProjectile && (Phase == EPhase::Perching || Phase == EPhase::Breathing)) return false;
	// only head hits deal full damage
	float A = Amount;
	if (D.Attacker.IsValid() && D.Attacker->IsA<AMCPlayer>() && !D.bProjectile)
	{
		const AMCPlayer* P = Cast<AMCPlayer>(D.Attacker.Get());
		TArray<FMCBox> Parts;
		GetPartBoxes(Parts);
		double T; EMCFace F;
		const bool bHead = Parts.Num() > 0 && Parts[0].Inflate(0.5).RayHit(P->GetEyePos(), P->GetLookDir(), 8.0, T, F);
		if (!bHead) A = A / 4.f + 1.f;
	}
	const float Before = Health;
	InvulnerableTime = 0; // the dragon has no hit immunity frames
	const bool bRes = AMCLiving::Hurt(D, A);
	if (bRes)
	{
		DamageSinceStateChange += Before - Health;
		PlaySound(TEXT("ender_dragon_hurt"), 5.f, 1.f);
		if (Phase == EPhase::Perching && DamageSinceStateChange > 20.f) SetPhase(EPhase::TakingOff);
	}
	return bRes;
}

void AMCEnderDragon::Die(const FMCDamage& D)
{
	if (Phase == EPhase::Dying) return;
	Health = 1.f; // stay "alive" through the death animation
	SetPhase(EPhase::Dying);
	DeathTicks = 0;
	PlaySound(TEXT("ender_dragon_death"), 10.f, 1.f);
	if (Game) Game->ScreenShake(0.6f);
}

void AMCEnderDragon::TickDeathSequence()
{
	++DeathTicks;
	Vel = FVector(0, 0, 0.1);
	Pos += Vel;
	if (World && DeathTicks % 3 == 0)
	{
		const FVector P = Pos + FVector(Rand().FRange(-4.f, 4.f), Rand().FRange(-4.f, 4.f), Rand().FRange(0.f, 6.f));
		World->SpawnParticles(TEXT("explosion_emitter"), P, 1, 0.f);
	}
	// XP orbs during the sequence
	if (World && DeathTicks > 150 && DeathTicks % 5 == 0 && Game)
	{
		const int32 Total = Game->DragonFight.bPreviouslyKilled ? 500 : 12000;
		World->SpawnXP(Pos + FVector(0, 0, 2), FMath::FloorToInt(Total * 0.08f));
	}
	if (DeathTicks >= 200)
	{
		if (Game) { Game->OnDragonKilled(); }
		Health = 0.f;
		Discard();
	}
}

void AMCEnderDragon::TickCrystalHealing()
{
	if (!World) return;
	AMCEntity* C = HealingCrystal.Get();
	if (C && C->bRemoved)
	{
		// the crystal healing us exploded
		HealingCrystal.Reset();
		FMCDamage D = FMCDamage::Explosion(C->Pos, nullptr);
		AMCLiving::Hurt(D, 10.f);
		C = nullptr;
	}
	if (C)
	{
		if (Age % 10 == 0 && Health < GetMaxHealth()) Health = FMath::Min(GetMaxHealth(), Health + 1.f);
		if (AMCEndCrystal* EC = Cast<AMCEndCrystal>(C)) { EC->bHasBeam = true; EC->BeamTarget = Pos + FVector(0, 0, 3.0); }
	}
	if (Age % 10 == 0 && Rand().NextInt(10) == 0)
	{
		// pick the nearest crystal within 32 blocks
		AMCEntity* Best = nullptr;
		double BestD = 32.0 * 32.0;
		for (AMCEntity* E : World->Entities)
		{
			if (!E || E->bRemoved || !E->IsA<AMCEndCrystal>()) continue;
			const double Dd = FVector::DistSquared(E->Pos, Pos);
			if (Dd < BestD) { BestD = Dd; Best = E; }
		}
		if (AMCEndCrystal* Old = Cast<AMCEndCrystal>(HealingCrystal.Get())) if (Old != Best) Old->bHasBeam = false;
		HealingCrystal = Best;
	}
}

void AMCEnderDragon::DestroyBlocksInBody()
{
	if (!World || !Game || !Game->Rules.bMobGriefing || Phase == EPhase::Perching || Phase == EPhase::Breathing || Phase == EPhase::Landing) return;
	TArray<FMCBox> Parts;
	GetPartBoxes(Parts);
	bool bAny = false;
	for (int32 i = 0; i < FMath::Min(3, Parts.Num()); ++i)
	{
		const FMCBox& B = Parts[i];
		for (int32 X = MC::FloorToInt(B.Min.X); X <= MC::FloorToInt(B.Max.X); ++X)
			for (int32 Y = MC::FloorToInt(B.Min.Y); Y <= MC::FloorToInt(B.Max.Y); ++Y)
				for (int32 Z = MC::FloorToInt(B.Min.Z); Z <= MC::FloorToInt(B.Max.Z); ++Z)
				{
					const FMCBlockPos P(X, Y, Z);
					const FMCState S = World->GetState(P);
					if (S == 0) continue;
					const FMCBlock& Blk = FMCBlocks::GetByState(S);
					if (IsDragonImmune(Blk) || FMCBlocks::IsFluid(S)) continue;
					World->SetState(P, 0, MCSet_Default);
					bAny = true;
				}
	}
	if (bAny && Rand().NextInt(4) == 0) World->SpawnParticles(TEXT("explosion"), Pos + FVector(0, 0, 3), 1, 2.f);
}

void AMCEnderDragon::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	SetActorRotation(FRotator::ZeroRotator);
	VisualRoot->SetWorldRotation(FRotator(-Pitch * 0.6f, FMath::Lerp(PrevYaw, PrevYaw + FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Alpha), 0.f));
	FMCRigPose Pose;
	Pose.Age = Age + Alpha;
	Pose.Special = FMath::Sin(FMath::Lerp(PrevWingFlap, WingFlap, Alpha));
	Pose.HeadPitch = Phase == EPhase::Breathing ? -30.f : Pitch;
	Pose.Hurt = HurtTime > 0 ? HurtTime / 10.f : 0.f;
	Pose.bSitting = Phase == EPhase::Perching || Phase == EPhase::Breathing;
	Pose.bFlying = true;
	Rig->ApplyPose(Pose);
	// wings flap with a large amplitude
	if (Rig->Rig)
	{
		const float Flap = FMath::Sin(FMath::Lerp(PrevWingFlap, WingFlap, Alpha));
		for (int32 i = 0; i < Rig->Rig->Parts.Num(); ++i)
		{
			const FMCRigPart& Part = Rig->Rig->Parts[i];
			if (Part.Role == EMCRigRole::WingR) Rig->SetPartRotation(i, FRotator(0, 0, -Flap * 35.f));
			else if (Part.Role == EMCRigRole::WingL) Rig->SetPartRotation(i, FRotator(0, 0, Flap * 35.f));
			else if (Part.Role == EMCRigRole::Jaw) Rig->SetPartRotation(i, FRotator(Phase == EPhase::Breathing || Phase == EPhase::Strafing ? 25.f : 3.f + FMath::Sin(Age * 0.1f) * 3.f, 0, 0));
		}
	}
	const bool bDying = Phase == EPhase::Dying;
	Rig->SetHurtFlash(Pose.Hurt > 0.f ? 0.5f : 0.f);
	Rig->SetGlow(bDying ? FMath::Min(1.f, DeathTicks / 200.f) : 0.f);
}

// =====================================================================================================================
// Wither

AMCWither::AMCWither()
{
	bPersistent = true;
}

void AMCWither::InitEntity()
{
	if (!Def) SetDefinition(MCMobs::Find(TEXT("wither")), 0);
	Super::InitEntity();
	bNoGravity = true;
	Gravity = 0.0;
	Health = GetMaxHealth() / 3.f;
	InvulTicks = 220;
	PlaySound(TEXT("wither_spawn"), 10.f, 1.f);
}

FVector AMCWither::HeadPos(int32 Index) const
{
	const FVector Fwd = FRotator(0, BodyYaw, 0).Vector();
	const FVector Right(-Fwd.Y, Fwd.X, 0);
	if (Index == 0) return Pos + FVector(0, 0, 3.0) + Fwd * 0.3;
	return Pos + FVector(0, 0, 2.2) + Right * (Index == 1 ? -1.3 : 1.3);
}

void AMCWither::AIStep()
{
	if (!World || !Game) return;
	if (InvulTicks > 0)
	{
		// charging up: regain health, then explode
		--InvulTicks;
		Health = FMath::Min(GetMaxHealth(), Health + GetMaxHealth() / 330.f);
		Vel = FVector::ZeroVector;
		if (InvulTicks == 0)
		{
			World->Explode(Pos + FVector(0, 0, EyeHeight), 7.f, false, Game->Rules.bMobGriefing, this);
			PlaySound(TEXT("wither_spawn"), 10.f, 1.f);
		}
		return;
	}
	// regeneration
	if (Age % 20 == 0) Heal(1.f);
	// targets: main head follows the player, side heads pick any living non-undead
	AMCPlayer* P = Game->Player && Game->Player->World == World && Game->Player->IsAlive() && !Game->Player->IsCreative() && !Game->Player->IsSpectator() ? Game->Player : nullptr;
	if (P && FVector::DistSquared(P->Pos, Pos) < 1024.0) HeadTarget[0] = P;
	else if (!HeadTarget[0].IsValid() || !HeadTarget[0]->IsAlive()) HeadTarget[0] = nullptr;
	for (int32 h = 1; h < 3; ++h)
	{
		if (Age % 20 == h * 5 && (!HeadTarget[h].IsValid() || !HeadTarget[h]->IsAlive()))
		{
			HeadTarget[h] = FindNearestMob(20.f, [](AMCLiving* L) { return !L->bUndead && L->IsAlive() && !L->IsA<AMCWither>(); });
			if (!HeadTarget[h].IsValid()) HeadTarget[h] = P;
		}
	}
	AMCLiving* Main = HeadTarget[0].Get();
	// hover above the main target (lower when armoured, i.e. below half health)
	if (Main)
	{
		const double Hover = IsPowered() ? 0.0 : 5.0;
		FVector Want = Main->Pos + FVector(0, 0, Hover);
		const FVector D = Want - Pos;
		const double HDist = FVector(D.X, D.Y, 0).Size();
		if (HDist > 9.0) Vel += FVector(D.X, D.Y, 0).GetSafeNormal() * 0.06;
		Vel.Z += (FMath::Clamp(D.Z, -1.0, 1.0) * 0.6 - Vel.Z) * 0.3;
		LookAt(Main->GetEyePos(), 20.f, 20.f);
	}
	else
	{
		Vel.Z *= 0.6;
		if (Rand().NextInt(40) == 0) Vel += FVector(Rand().FRange(-0.3f, 0.3f), Rand().FRange(-0.3f, 0.3f), 0.0);
	}
	Vel.X *= 0.9; Vel.Y *= 0.9;
	Move(Vel);
	BodyYaw = Yaw;
	// shooting
	for (int32 h = 0; h < 3; ++h)
	{
		if (HeadCooldown[h] > 0) { --HeadCooldown[h]; continue; }
		AMCLiving* T = HeadTarget[h].Get();
		if (!T || !CanSee(T) || FVector::DistSquared(T->Pos, Pos) > 1024.0) continue;
		const FVector From = HeadPos(h);
		AMCProjectile* S = Game->SpawnEntity<AMCProjectile>(World, From);
		if (!S) continue;
		S->Type = EMCProjectile::WitherSkull;
		S->Shooter = this;
		S->bPickup = false;
		S->Damage = 8.f;
		S->bDangerous = Rand().NextInt(1000) == 0 || (Game->Difficulty >= EMCDifficulty::Normal && Rand().NextInt(100) == 0 && h == 0);
		S->InitEntity();
		const FVector Dir = (T->GetEyePos() - From).GetSafeNormal();
		S->Shoot(Dir, S->bDangerous ? 0.4f : 0.9f, 0.f);
		S->Accel = Dir * 0.1;
		PlaySound(TEXT("wither_shoot"), 3.f, 1.f);
		HeadCooldown[h] = h == 0 ? (Game->Difficulty == EMCDifficulty::Hard ? 20 : 40) : 40 + Rand().NextInt(20);
	}
	// break blocks around the body after being hurt
	if (BlockBreakCooldown > 0) --BlockBreakCooldown;
	if (BlockBreakCooldown == 0 && HurtTime > 0 && Game->Rules.bMobGriefing)
	{
		BlockBreakCooldown = 20;
		bool bAny = false;
		for (int32 dx = -1; dx <= 1; ++dx) for (int32 dy = -1; dy <= 1; ++dy) for (int32 dz = 0; dz <= 3; ++dz)
		{
			const FMCBlockPos B = BlockPos() + FIntVector(dx, dy, dz);
			const FMCState S = World->GetState(B);
			if (S == 0 || FMCBlocks::IsFluid(S) || IsWitherImmune(FMCBlocks::GetByState(S))) continue;
			World->DestroyBlock(B, true, this);
			bAny = true;
		}
		if (bAny) PlaySound(TEXT("wither_break_block"), 1.f, 1.f);
	}
	if (Age % 60 == 0 && Rand().NextInt(2) == 0) PlaySound(TEXT("wither_ambient"), 3.f, 1.f);
}

bool AMCWither::Hurt(const FMCDamage& D, float Amount)
{
	if (InvulTicks > 0 && !D.bBypassInvulnerability) return false;
	if (D.Type == TEXT("drown") || D.Type == TEXT("wither")) return false;
	if (IsPowered() && D.bProjectile) return false; // wither armour deflects arrows
	if (D.Attacker.IsValid() && D.Attacker->IsA<AMCWither>()) return false;
	const bool bRes = Super::Hurt(D, Amount);
	if (bRes) BlockBreakCooldown = FMath::Min(BlockBreakCooldown, 5);
	return bRes;
}

void AMCWither::Die(const FMCDamage& D)
{
	if (DeathTime > 0) return;
	PlaySound(TEXT("wither_death"), 10.f, 1.f);
	AMCLiving::Die(D);
	if (World)
	{
		World->SpawnItem(Pos + FVector(0, 0, 1.5), FMCItemStack::Of(TEXT("nether_star"), 1), true, 0.5f);
		World->SpawnXP(Pos, 50);
	}
}

void AMCWither::UpdateVisual(float Alpha, float DeltaSeconds)
{
	Super::UpdateVisual(Alpha, DeltaSeconds);
	Rig->SetGlow(InvulTicks > 0 ? (FMath::Sin(Age * 0.5f) * 0.5f + 0.5f) : 0.f);
	if (Rig->Rig)
	{
		for (int32 i = 0; i < Rig->Rig->Parts.Num(); ++i)
		{
			const FMCRigPart& Part = Rig->Rig->Parts[i];
			if (Part.Role != EMCRigRole::Head || Part.Index == 0) continue;
			const AMCLiving* T = HeadTarget[FMath::Clamp(Part.Index, 1, 2)].Get();
			if (!T) continue;
			const FVector D = T->GetEyePos() - HeadPos(Part.Index);
			const float TYaw = FMath::FindDeltaAngleDegrees(BodyYaw, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
			const float TPitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, FVector(D.X, D.Y, 0).Size()));
			Rig->SetPartRotation(i, FRotator(-TPitch, FMath::Clamp(TYaw, -80.f, 80.f), 0));
		}
	}
}

bool AMCWither::TrySpawnFromStructure(FMCWorld& W, const FMCBlockPos& SkullPos)
{
	if (!W.Game) return false;
	auto Is = [&](const FMCBlockPos& P, bool bSkull)
	{
		const FName N = FMCBlocks::GetByState(W.GetState(P)).Name;
		return bSkull ? (N == TEXT("wither_skeleton_skull") || N == TEXT("wither_skeleton_wall_skull")) : (N == TEXT("soul_sand") || N == TEXT("soul_soil"));
	};
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FIntVector A = Axis == 0 ? FIntVector(1, 0, 0) : FIntVector(0, 1, 0);
		for (int32 Off = -1; Off <= 1; ++Off)
		{
			// Off = position of the placed skull within the top row
			const FMCBlockPos Center = SkullPos + FIntVector(-A.X * Off, -A.Y * Off, 0);
			const FMCBlockPos L = Center + FIntVector(-A.X, -A.Y, 0), R = Center + A;
			if (!Is(Center, true) || !Is(L, true) || !Is(R, true)) continue;
			const FMCBlockPos B1 = Center.Down(), B1L = L.Down(), B1R = R.Down(), B2 = Center.Down(2);
			if (!Is(B1, false) || !Is(B1L, false) || !Is(B1R, false) || !Is(B2, false)) continue;
			// bottom corners must not be soul sand (pattern requires air there)
			if (Is(L.Down(2), false) || Is(R.Down(2), false)) continue;
			for (const FMCBlockPos& P : { Center, L, R, B1, B1L, B1R, B2 })
			{
				W.SpawnBlockBreakParticles(P, W.GetState(P));
				W.SetState(P, 0, MCSet_Default);
			}
			if (AMCEntity* E = W.Game->SpawnEntity<AMCWither>(&W, FVector(B2.X + 0.5, B2.Y + 0.5, B2.Z)))
			{
				E->Yaw = Axis == 0 ? 90.f : 0.f;
			}
			return true;
		}
	}
	return false;
}
