// Projectiles: arrows, tridents, throwables, potions, fireballs, wither skulls, shulker bullets, wind charges, fishing bobbers.
#include "Game/MCEntities.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCGame.h"
#include "Render/MCRig.h"
#include "Render/MCAssets.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Components/PointLightComponent.h"

AMCProjectile::AMCProjectile()
{
	Kind = EMCEntityKind::Projectile;
	TypeId = TEXT("projectile");
	Width = Height = 0.25f;
	bNoPhysics = true;
	StepHeight = 0.f;
	Visual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("ItemModel")); // "Visual" is AMCEntity::VisualRoot
	Visual->SetupAttachment(VisualRoot);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(VisualRoot);
	Light->SetVisibility(false);
	Light->SetCastShadows(false);
	Light->SetIntensity(4000.f);
	Light->SetAttenuationRadius(700.f);
}

float AMCProjectile::GetGravity() const
{
	switch (Type)
	{
	case EMCProjectile::Arrow: case EMCProjectile::SpectralArrow: case EMCProjectile::Trident: case EMCProjectile::Spear: return 0.05f;
	case EMCProjectile::Potion: case EMCProjectile::LingeringPotion: return 0.05f;
	case EMCProjectile::ExpBottle: return 0.07f;
	case EMCProjectile::LlamaSpit: return 0.06f;
	case EMCProjectile::FishingBobber: return 0.04f;
	case EMCProjectile::Fireball: case EMCProjectile::SmallFireball: case EMCProjectile::DragonFireball: case EMCProjectile::WitherSkull:
	case EMCProjectile::ShulkerBullet: case EMCProjectile::WindCharge: case EMCProjectile::BreezeWindCharge: return 0.f;
	default: return 0.03f;
	}
}

float AMCProjectile::GetDrag() const
{
	const bool bArrow = Type == EMCProjectile::Arrow || Type == EMCProjectile::SpectralArrow;
	if (bInWater) return Type == EMCProjectile::Trident ? 0.99f : (bArrow ? 0.6f : 0.8f);
	switch (Type)
	{
	case EMCProjectile::Fireball: case EMCProjectile::SmallFireball: case EMCProjectile::DragonFireball: case EMCProjectile::WitherSkull: return 0.95f;
	case EMCProjectile::WindCharge: case EMCProjectile::BreezeWindCharge: return 1.f;
	case EMCProjectile::FishingBobber: return 0.92f;
	default: return 0.99f;
	}
}

bool AMCProjectile::IsPickable() const
{
	return Type == EMCProjectile::Fireball || Type == EMCProjectile::SmallFireball || Type == EMCProjectile::ShulkerBullet || Type == EMCProjectile::WitherSkull || Type == EMCProjectile::DragonFireball || Type == EMCProjectile::WindCharge;
}

void AMCProjectile::InitEntity()
{
	FMCItemStack Show;
	float Scale = 0.5f;
	Light->SetVisibility(false);
	switch (Type)
	{
	case EMCProjectile::Arrow: Show = Item.IsEmpty() ? FMCItemStack::Of(TEXT("arrow"), 1) : Item; Width = Height = 0.5f; Scale = 0.6f; break;
	case EMCProjectile::SpectralArrow: Show = FMCItemStack::Of(TEXT("spectral_arrow"), 1); Width = Height = 0.5f; Scale = 0.6f; break;
	case EMCProjectile::Trident: Show = Item.IsEmpty() ? FMCItemStack::Of(TEXT("trident"), 1) : Item; Width = Height = 0.5f; Scale = 1.0f; break;
	case EMCProjectile::Fireball: Show = FMCItemStack::Of(TEXT("fire_charge"), 1); Width = Height = 1.f; Scale = 1.2f; Light->SetVisibility(true); Light->SetLightColor(FLinearColor(1.f, 0.5f, 0.1f)); bFireImmune = true; break;
	case EMCProjectile::SmallFireball: Show = FMCItemStack::Of(TEXT("fire_charge"), 1); Width = Height = 0.3125f; Scale = 0.5f; Light->SetVisibility(true); Light->SetLightColor(FLinearColor(1.f, 0.6f, 0.2f)); bFireImmune = true; break;
	case EMCProjectile::DragonFireball: Show = FMCItemStack::Of(TEXT("dragon_breath"), 1); Width = Height = 1.f; Scale = 1.2f; Light->SetVisibility(true); Light->SetLightColor(FLinearColor(0.8f, 0.2f, 1.f)); bFireImmune = true; break;
	case EMCProjectile::WitherSkull:
		Width = Height = 0.3125f;
		Visual->SetBlockState(FMCBlocks::FindState(TEXT("wither_skeleton_skull")), 0.6f);
		if (bDangerous) { Light->SetVisibility(true); Light->SetLightColor(FLinearColor(0.3f, 0.5f, 1.f)); }
		bFireImmune = true;
		return;
	case EMCProjectile::ShulkerBullet: Show = FMCItemStack::Of(TEXT("end_rod"), 1); Width = Height = 0.3125f; Scale = 0.4f; Light->SetVisibility(true); Light->SetLightColor(FLinearColor(1.f, 0.95f, 0.8f)); break;
	case EMCProjectile::LlamaSpit: Width = Height = 0.25f; break;
	case EMCProjectile::WindCharge: case EMCProjectile::BreezeWindCharge: Show = FMCItemStack::Of(TEXT("wind_charge"), 1); Width = Height = 0.3125f; Scale = 0.5f; break;
	case EMCProjectile::FishingBobber: Show = FMCItemStack::Of(TEXT("fishing_rod"), 1); Width = Height = 0.25f; Scale = 0.25f; break;
	default: Show = Item; break;
	}
	if (!Show.IsEmpty())
	{
		Visual->SetStack(Show, 0);
		Visual->SetRelativeScale3D(FVector(Scale));
	}
}

void AMCProjectile::Shoot(const FVector& Dir, float Speed, float Inaccuracy)
{
	FVector D = Dir.GetSafeNormal();
	D += FVector(Rand().Gaussian(), Rand().Gaussian(), Rand().Gaussian()) * 0.0075 * Inaccuracy;
	Vel = D * Speed;
	Yaw = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X));
	Pitch = FMath::RadiansToDegrees(FMath::Atan2(Vel.Z, FVector(Vel.X, Vel.Y, 0).Size()));
	PrevYaw = Yaw; PrevPitch = Pitch;
}

bool AMCProjectile::ShouldHit(AMCEntity* E) const
{
	if (!E || E == this || E->bRemoved || !E->IsLiving() && !E->IsA<AMCEndCrystal>() && !E->IsA<AMCBoat>() && !E->IsA<AMCMinecart>() && !E->IsA<AMCProjectile>() && !E->IsA<AMCItemFrame>()) return false;
	if (const AMCProjectile* P = Cast<AMCProjectile>(E)) if (!P->IsPickable() || P->Shooter == Shooter) return false;
	if (E == Shooter.Get() && Age < 5) return false;
	if (const AMCPlayer* PL = Cast<AMCPlayer>(E)) if (PL->IsSpectator()) return false;
	if (const AMCLiving* L = Cast<AMCLiving>(E)) if (!L->IsAlive()) return false;
	if (PiercedIds.Contains(E)) return false;
	if (Type == EMCProjectile::FishingBobber && E == Shooter.Get()) return false;
	return true;
}

void AMCProjectile::TickEntity()
{
	PrevPos = Pos;
	PrevYaw = Yaw; PrevPitch = Pitch;
	++Age;
	if (!World) return;
	UpdateFluidState();
	if (bInWater && IsOnFire()) Extinguish();
	if (Shake > 0) --Shake;

	// ---------------------------------------------------------------- stuck in a block
	if (bInGround)
	{
		++LifeInGround;
		const FMCBlockPos In(MC::FloorToInt(Pos.X + Vel.X * 0.01), MC::FloorToInt(Pos.Y + Vel.Y * 0.01), MC::FloorToInt(Pos.Z + Vel.Z * 0.01));
		// block removed: start falling again
		if (Age % 5 == 0)
		{
			const FVector Probe = Pos + Vel.GetSafeNormal() * 0.1;
			const FMCBlockPos PB(MC::FloorToInt(Probe.X), MC::FloorToInt(Probe.Y), MC::FloorToInt(Probe.Z));
			if (World->GetState(PB) == 0) { bInGround = false; Vel = FVector(Rand().FRange(-0.02f, 0.02f), Rand().FRange(-0.02f, 0.02f), 0.0); LifeInGround = 0; }
		}
		(void)In;
		if (Type == EMCProjectile::Trident && Loyalty > 0 && Shooter.IsValid()) { bInGround = false; bReturning = true; }
		if ((Type == EMCProjectile::Arrow || Type == EMCProjectile::SpectralArrow) && LifeInGround >= 1200) Discard();
		return;
	}

	// ---------------------------------------------------------------- loyalty return
	if (bReturning && Type == EMCProjectile::Trident)
	{
		AMCEntity* S = Shooter.Get();
		if (!S || S->bRemoved) { bReturning = false; }
		else
		{
			const FVector To = S->GetEyePos() - Pos;
			Pos += To.GetSafeNormal() * FMath::Min(To.Size(), 0.05 * Loyalty * 3.0 + 0.3);
			Vel = To.GetSafeNormal() * 0.5;
			if (Age % 4 == 0 && Age % 40 == 0) PlaySound(TEXT("trident_return"), 1.f, 1.f);
			if (To.SizeSquared() < 1.5) OnPlayerTouch(Cast<AMCPlayer>(S));
			return;
		}
	}

	// ---------------------------------------------------------------- fishing bobber floating
	if (Type == EMCProjectile::FishingBobber)
	{
		AMCPlayer* Angler = Cast<AMCPlayer>(Shooter.Get());
		if (!Angler || Angler->bRemoved || FVector::DistSquared(Angler->Pos, Pos) > 1024.0 || Angler->HeldConst().Item().Kind != EMCItemKind::FishingRod) { Discard(); return; }
		if (AMCEntity* Hooked = HomingTarget.Get()) { Pos = Hooked->Pos + FVector(0, 0, Hooked->Height * 0.8); Vel = FVector::ZeroVector; return; }
		double Surface = 0.0;
		const bool bWater = World->IsInFluid(GetBox().Inflate(0.1), FMCBlocks::C.WaterId, &Surface);
		if (bWater)
		{
			Vel.X *= 0.9; Vel.Y *= 0.9;
			Vel.Z += (Surface - 0.1 - Pos.Z) * 0.1 - Vel.Z * 0.2;
			Pos += Vel;
			// fish bites: random timer, lure makes it faster
			if (Shake <= 0)
			{
				if (Rand().NextInt(FMath::Max(1, 600 - 100 * Angler->HeldConst().GetEnchant(EMCEnchant::Lure))) == 0)
				{
					Shake = 25;
					Vel.Z -= 0.2;
					World->PlaySound(TEXT("fishing_bobber_splash"), Pos, 0.25f, 1.f + (Rand().NextFloat() - Rand().NextFloat()) * 0.4f);
					World->SpawnParticles(TEXT("splash"), Pos, 12, 0.3f, FVector(0, 0, 0.2), FColor(200, 220, 255));
				}
			}
			return;
		}
	}

	// ---------------------------------------------------------------- homing shulker bullet
	if (Type == EMCProjectile::ShulkerBullet)
	{
		if (AMCEntity* T = HomingTarget.Get())
		{
			const FVector To = (T->Pos + FVector(0, 0, T->Height * 0.5)) - Pos;
			Vel += (To.GetSafeNormal() * 0.3 - Vel) * 0.15;
		}
		if (Age > 400) { Discard(); return; }
		World->SpawnParticles(TEXT("end_rod"), Pos, 1, 0.05f);
	}

	// ---------------------------------------------------------------- sweep along the velocity
	const FVector Start = Pos;
	const double Len = Vel.Size();
	if (Len < 1e-6) { Vel.Z -= GetGravity(); return; }
	const FVector Dir = Vel / Len;
	FMCRayHit Hit;
	const bool bBlock = World->Raycast(Start, Dir, Len, Hit, false, false);
	double HitT = bBlock ? Hit.Distance : Len;
	AMCEntity* HitEntity = nullptr;
	{
		TArray<AMCEntity*> Near;
		const FVector End = Start + Dir * HitT;
		const FMCBox Sweep(FVector(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y), FMath::Min(Start.Z, End.Z)), FVector(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y), FMath::Max(Start.Z, End.Z)));
		World->GetEntitiesInBox(Sweep.Inflate(1.0), Near, this);
		for (AMCEntity* E : Near)
		{
			if (!ShouldHit(E)) continue;
			double T = 0.0; EMCFace F;
			bool bOk;
			if (const AMCEnderDragon* Dr = Cast<AMCEnderDragon>(E)) bOk = Dr->RayHitParts(Start, Dir, HitT, T);
			else bOk = E->GetBox().Inflate(0.3).RayHit(Start, Dir, HitT, T, F);
			if (bOk && T < HitT) { HitT = T; HitEntity = E; }
		}
	}
	if (HitEntity)
	{
		Pos = Start + Dir * HitT;
		OnHitEntity(HitEntity);
		if (bRemoved) return;
	}
	else if (bBlock)
	{
		Pos = Hit.Point - Dir * 0.05;
		OnHitBlock(Hit);
		if (bRemoved || bInGround) return;
	}
	else
	{
		Pos = Start + Vel;
	}

	// ---------------------------------------------------------------- forces
	const float Drag = GetDrag();
	Vel *= Drag;
	Vel += Accel;
	Vel.Z -= GetGravity();
	Yaw = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X));
	Pitch = FMath::RadiansToDegrees(FMath::Atan2(Vel.Z, FVector(Vel.X, Vel.Y, 0).Size()));
	if (bInWater && (Type == EMCProjectile::Arrow || Type == EMCProjectile::SpectralArrow)) World->SpawnParticles(TEXT("bubble"), Pos, 1, 0.05f);

	// ---------------------------------------------------------------- trails
	switch (Type)
	{
	case EMCProjectile::Arrow: case EMCProjectile::SpectralArrow:
		if (bCritical) World->SpawnParticles(TEXT("crit"), Pos, 2, 0.05f);
		if (Item.Extra.IsValid() && Item.Extra->Potion > 0 && Age % 2 == 0) World->SpawnParticles(TEXT("effect"), Pos, 1, 0.05f, FVector::ZeroVector, MCPotions::Get(Item.Extra->Potion).Color);
		break;
	case EMCProjectile::Fireball: case EMCProjectile::SmallFireball: World->SpawnParticles(TEXT("smoke"), Pos, 1, 0.05f); World->SpawnParticles(TEXT("flame"), Pos, 1, 0.1f); break;
	case EMCProjectile::DragonFireball: World->SpawnParticles(TEXT("dragon_breath"), Pos, 2, 0.2f, FVector::ZeroVector, FColor(200, 60, 230)); break;
	case EMCProjectile::WitherSkull: World->SpawnParticles(TEXT("smoke"), Pos, 1, 0.05f); break;
	case EMCProjectile::LlamaSpit: World->SpawnParticles(TEXT("spit"), Pos, 2, 0.05f, FVector::ZeroVector, FColor(240, 240, 240)); break;
	case EMCProjectile::WindCharge: case EMCProjectile::BreezeWindCharge: World->SpawnParticles(TEXT("gust"), Pos, 1, 0.05f, FVector::ZeroVector, FColor(220, 230, 255)); break;
	case EMCProjectile::Potion: case EMCProjectile::LingeringPotion: if (Age % 2 == 0) World->SpawnParticles(TEXT("effect"), Pos, 1, 0.05f, FVector::ZeroVector, Item.Extra.IsValid() ? MCPotions::Get(Item.Extra->Potion).Color : FColor(56, 93, 198)); break;
	default: break;
	}
	if (Age > 1200 || Pos.Z < MC::MinZ - 64) Discard();
}

void AMCProjectile::OnHitBlock(const FMCRayHit& Hit)
{
	if (!World) return;
	const FMCState S = Hit.State;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	B.Behavior->OnProjectileHit(*World, Hit.Pos, S, this, Hit.Point);
	const FVector P = Hit.Point;
	switch (Type)
	{
	case EMCProjectile::Arrow:
	case EMCProjectile::SpectralArrow:
	case EMCProjectile::Trident:
		bInGround = true;
		Shake = 7;
		Vel = (Hit.Point - PrevPos).GetSafeNormal() * 0.05;
		Pos = Hit.Point - Vel.GetSafeNormal() * 0.05;
		World->PlaySound(Type == EMCProjectile::Trident ? TEXT("trident_hit_ground") : TEXT("arrow_hit"), Pos, 1.f, 1.2f / (Rand().NextFloat() * 0.2f + 0.9f));
		if (bFlame && B.Name == TEXT("tnt")) B.Behavior->OnIgnite(*World, Hit.Pos, S, Shooter.Get());
		// channeling during thunder summons lightning onto lightning rods
		if (Type == EMCProjectile::Trident && Item.GetEnchant(EMCEnchant::Channeling) > 0 && Game && Game->bThundering && B.Name == TEXT("lightning_rod")) Game->StrikeLightning(World, FVector(Hit.Pos.X + 0.5, Hit.Pos.Y + 0.5, Hit.Pos.Z + 1), false);
		return;
	case EMCProjectile::Snowball:
		World->SpawnParticles(TEXT("item_snowball"), P, 8, 0.1f, FVector(0, 0, 0.1), FColor::White);
		break;
	case EMCProjectile::Egg:
		World->SpawnParticles(TEXT("item_egg"), P, 8, 0.1f, FVector(0, 0, 0.1), FColor(240, 230, 200));
		if (Game && Rand().NextInt(8) == 0)
		{
			const int32 N = Rand().NextInt(32) == 0 ? 4 : 1;
			for (int32 i = 0; i < N; ++i) if (AMCMob* C = Cast<AMCMob>(Game->SpawnMob(World, TEXT("chicken"), Pos, false))) C->SetBaby(true);
		}
		break;
	case EMCProjectile::EnderPearl:
	{
		AMCEntity* Sh = Shooter.Get();
		World->SpawnParticles(TEXT("portal"), P, 32, 0.5f, FVector::ZeroVector, FColor(150, 60, 220));
		if (Sh && Sh->World == World && !Sh->bRemoved)
		{
			if (Rand().NextInt(20) == 0 && Game) Game->SpawnMob(World, TEXT("endermite"), Sh->Pos, false);
			Sh->StopRiding();
			const FVector Dest = Hit.Point + FVector(MC::FaceDir[(int32)Hit.Face]) * 0.5 - FVector(0, 0, Hit.Face == EMCFace::Down ? Sh->Height : 0.0);
			Sh->TeleportTo(FVector(Dest.X, Dest.Y, Hit.Face == EMCFace::Up ? Hit.Point.Z : Dest.Z));
			Sh->Hurt(FMCDamage::Of(TEXT("fall")), 5.f);
			World->PlaySound(TEXT("ender_pearl_teleport"), Sh->Pos, 1.f, 1.f);
		}
		break;
	}
	case EMCProjectile::ExpBottle:
		World->SpawnParticles(TEXT("splash_potion"), P, 30, 0.6f, FVector(0, 0, 0.2), FColor(80, 120, 255));
		World->PlaySound(TEXT("splash_potion_break"), P, 1.f, 1.f);
		World->SpawnXP(P, 3 + Rand().NextInt(5) + Rand().NextInt(5));
		break;
	case EMCProjectile::Potion:
	case EMCProjectile::LingeringPotion:
		OnHitEntity(nullptr);
		return;
	case EMCProjectile::Fireball:
		World->Explode(P, 1.f, true, !Game || Game->Rules.bMobGriefing, this);
		break;
	case EMCProjectile::SmallFireball:
	{
		const FMCBlockPos F = Hit.Pos.Offset(Hit.Face);
		if ((!Game || Game->Rules.bMobGriefing || Shooter.IsValid() && Shooter->IsA<AMCPlayer>()) && World->GetState(F) == 0) World->SetState(F, FMCBlocks::C.Fire, MCSet_Default);
		break;
	}
	case EMCProjectile::DragonFireball:
		if (Game)
		{
			if (AMCAreaCloud* C = Game->SpawnEntity<AMCAreaCloud>(World, P))
			{
				C->bDragonBreath = true; C->Radius = 3.f; C->Duration = 600; C->Color = FColor(180, 40, 220); C->Owner = Shooter;
			}
		}
		World->PlaySound(TEXT("dragon_fireball_explode"), P, 1.f, 1.f);
		break;
	case EMCProjectile::WitherSkull:
		World->Explode(P, 1.f, false, !Game || Game->Rules.bMobGriefing, this);
		break;
	case EMCProjectile::ShulkerBullet:
		World->SpawnParticles(TEXT("explosion"), P, 1, 0.f);
		World->PlaySound(TEXT("shulker_bullet_hit"), P, 1.f, 1.f);
		break;
	case EMCProjectile::WindCharge:
	case EMCProjectile::BreezeWindCharge:
		OnHitEntity(nullptr);
		return;
	case EMCProjectile::FishingBobber:
		Vel = FVector::ZeroVector;
		return;
	default: break;
	}
	Discard();
}

void AMCProjectile::OnHitEntity(AMCEntity* E)
{
	if (!World) return;
	AMCEntity* Sh = Shooter.Get();
	auto SplashPotion = [&](bool bLinger)
	{
		const int32 PIdx = Item.Extra.IsValid() ? Item.Extra->Potion : 0;
		const MCPotions::FPotionDef& Def = MCPotions::Get(PIdx);
		World->SpawnParticles(TEXT("splash_potion"), Pos, 40, 1.5f, FVector(0, 0, 0.2), Def.Color);
		World->PlaySound(TEXT("splash_potion_break"), Pos, 1.f, 1.f);
		if (bLinger && Game)
		{
			if (AMCAreaCloud* C = Game->SpawnEntity<AMCAreaCloud>(World, Pos))
			{
				C->Effect = Def.Effect; C->EffectDuration = Def.Duration; C->Amplifier = Def.Amplifier; C->Radius = 3.f; C->Duration = 600; C->Color = Def.Color; C->Owner = Sh;
			}
			return;
		}
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(FMCBox(Pos - FVector(4, 4, 2), Pos + FVector(4, 4, 2)), Near, nullptr);
		const bool bWater = FCString::Strcmp(Def.Id, TEXT("water")) == 0;
		for (AMCEntity* O : Near)
		{
			AMCLiving* L = Cast<AMCLiving>(O);
			if (!L) continue;
			const double D = FVector::Dist(L->Pos, Pos);
			if (D > 4.0) continue;
			if (bWater) { L->Extinguish(); if (L->IsSensitiveToWater()) L->Hurt(FMCDamage::Of(TEXT("drown")), 1.f); continue; }
			if (Def.Effect == EMCEffect::None) continue;
			const double F = 1.0 - D / 4.0 * (O == E ? 0.0 : 1.0);
			if (MCEffects::IsInstant(Def.Effect)) L->ApplyInstantEffect(Def.Effect, Def.Amplifier, Sh);
			else
			{
				FMCEffectInstance I; I.Effect = Def.Effect; I.Duration = FMath::RoundToInt(Def.Duration * F); I.Amplifier = Def.Amplifier;
				if (I.Duration > 20) L->AddEffect(I);
			}
		}
		if (bWater)
		{
			// put out fires around the impact
			const FMCBlockPos C = BlockPos();
			for (int32 dx = -1; dx <= 1; ++dx) for (int32 dy = -1; dy <= 1; ++dy) for (int32 dz = -1; dz <= 1; ++dz)
			{
				const FMCBlockPos P = C + FIntVector(dx, dy, dz);
				const FMCState S = World->GetState(P);
				if (S == FMCBlocks::C.Fire || FMCBlocks::GetByState(S).Name == TEXT("soul_fire")) World->SetState(P, 0);
			}
		}
	};
	auto WindBurst = [&]()
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(FMCBox(Pos - FVector(2.5), Pos + FVector(2.5)), Near, this);
		for (AMCEntity* O : Near)
		{
			if (O->IsA<AMCProjectile>() || O->bRemoved) continue;
			FVector D = O->Pos + FVector(0, 0, O->Height * 0.5) - Pos;
			const double Dist = D.Size();
			if (Dist > 2.5) continue;
			const double K = (1.0 - Dist / 2.5) * (Type == EMCProjectile::WindCharge ? 1.2 : 0.9);
			D = D.GetSafeNormal();
			O->Vel += FVector(D.X * K, D.Y * K, FMath::Max(0.2, D.Z) * K + 0.3);
			O->FallDistance = 0.f;
			if (O != Sh && O->IsLiving() && Type == EMCProjectile::BreezeWindCharge) O->Hurt(FMCDamage::Projectile(this, Sh, TEXT("wind_charge")), 1.f);
		}
		World->SpawnParticles(TEXT("gust_emitter"), Pos, 1, 0.f);
		World->PlaySound(TEXT("wind_charge_burst"), Pos, 1.f, 1.f);
	};

	switch (Type)
	{
	case EMCProjectile::Potion: SplashPotion(false); Discard(); return;
	case EMCProjectile::LingeringPotion: SplashPotion(true); Discard(); return;
	case EMCProjectile::WindCharge: case EMCProjectile::BreezeWindCharge: WindBurst(); Discard(); return;
	default: break;
	}
	if (!E) { Discard(); return; }

	switch (Type)
	{
	case EMCProjectile::Arrow:
	case EMCProjectile::SpectralArrow:
	{
		const double Speed = Vel.Size();
		int32 Dmg = FMath::CeilToInt(FMath::Clamp(Speed * Damage, 0.0, 2147483647.0));
		if (bCritical) Dmg += Rand().NextInt(Dmg / 2 + 2);
		if (IsOnFire() && !E->IsA<AMCEnderDragon>()) E->SetOnFire(5);
		FMCDamage D = FMCDamage::Projectile(this, Sh, TEXT("arrow"));
		const bool bHit = E->Hurt(D, (float)Dmg);
		if (bHit)
		{
			if (AMCLiving* L = Cast<AMCLiving>(E))
			{
				if (Knockback > 0) { const FVector H = FVector(Vel.X, Vel.Y, 0).GetSafeNormal() * Knockback * 0.6; L->Vel += FVector(H.X, H.Y, 0.1); }
				if (Item.Extra.IsValid() && Item.Extra->Potion > 0)
				{
					const MCPotions::FPotionDef& P = MCPotions::Get(Item.Extra->Potion);
					if (P.Effect != EMCEffect::None) { FMCEffectInstance I; I.Effect = P.Effect; I.Duration = FMath::Max(1, P.Duration / 8); I.Amplifier = P.Amplifier; L->AddEffect(I); }
				}
				if (Type == EMCProjectile::SpectralArrow) { FMCEffectInstance I; I.Effect = EMCEffect::Glowing; I.Duration = 200; L->AddEffect(I); }
			}
			World->PlaySound(TEXT("arrow_hit"), Pos, 1.f, 1.2f / (Rand().NextFloat() * 0.2f + 0.9f));
			if (Sh && Sh->IsA<AMCPlayer>() && E->IsA<AMCPlayer>() == false) Sh->PlaySound(TEXT("arrow_hit_player"), 0.2f, 0.5f);
			if (Pierce > 0 && PiercedIds.Num() < Pierce) { PiercedIds.Add(E); return; }
			Discard();
		}
		else
		{
			// deflected: bounce back
			Vel *= -0.1;
			Yaw += 180.f;
			if (Type == EMCProjectile::Arrow && !E->IsA<AMCEnderDragon>()) {}
		}
		return;
	}
	case EMCProjectile::Trident:
	{
		float Dmg = Damage;
		if (AMCLiving* L = Cast<AMCLiving>(E))
		{
			const int32 Imp = Item.GetEnchant(EMCEnchant::Impaling);
			if (Imp > 0 && (L->bInWater || (Game && Game->IsRainingAt(L->BlockPos())))) Dmg += 2.5f * Imp;
		}
		if (E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("trident")), Dmg))
		{
			World->PlaySound(TEXT("trident_hit"), Pos, 1.f, 1.f);
			if (Item.GetEnchant(EMCEnchant::Channeling) > 0 && Game && Game->bThundering && World->CanSeeSky(E->BlockPos())) Game->StrikeLightning(World, E->Pos, false);
		}
		Vel = FVector(-Vel.X * 0.01, -Vel.Y * 0.01, -0.1);
		PiercedIds.Add(E);
		if (Loyalty > 0) bReturning = true;
		return;
	}
	case EMCProjectile::Snowball:
		E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("thrown")), (E->IsA<AMCMob>() && Cast<AMCMob>(E)->Def && Cast<AMCMob>(E)->Def->Id == TEXT("blaze")) ? 3.f : 0.001f);
		World->SpawnParticles(TEXT("item_snowball"), Pos, 8, 0.1f, FVector(0, 0, 0.1), FColor::White);
		break;
	case EMCProjectile::Egg:
		E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("thrown")), 0.001f);
		World->SpawnParticles(TEXT("item_egg"), Pos, 8, 0.1f, FVector(0, 0, 0.1), FColor(240, 230, 200));
		break;
	case EMCProjectile::EnderPearl:
	{
		FMCRayHit H; H.bHit = true; H.Point = Pos; H.Pos = BlockPos(); H.Face = EMCFace::Up; H.State = 0;
		E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("thrown")), 0.001f);
		OnHitBlock(H);
		return;
	}
	case EMCProjectile::ExpBottle:
	{
		FMCRayHit H; H.bHit = true; H.Point = Pos; H.Pos = BlockPos(); H.State = 0;
		OnHitBlock(H);
		return;
	}
	case EMCProjectile::Fireball:
		E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("fireball")), 6.f);
		World->Explode(Pos, 1.f, true, !Game || Game->Rules.bMobGriefing, this);
		break;
	case EMCProjectile::SmallFireball:
		if (!E->bFireImmune) { if (E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("fireball")), 5.f)) E->SetOnFire(5); }
		break;
	case EMCProjectile::DragonFireball:
	{
		FMCRayHit H; H.bHit = true; H.Point = Pos; H.Pos = BlockPos(); H.State = 0;
		OnHitBlock(H);
		return;
	}
	case EMCProjectile::WitherSkull:
	{
		const bool bHit = E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("wither_skull")), Damage);
		if (bHit)
		{
			if (AMCLiving* L = Cast<AMCLiving>(E))
			{
				const int32 Secs = Game && Game->Difficulty == EMCDifficulty::Hard ? 40 : (Game && Game->Difficulty == EMCDifficulty::Normal ? 10 : 0);
				if (Secs > 0) { FMCEffectInstance I; I.Effect = EMCEffect::Wither; I.Duration = Secs * 20; I.Amplifier = 1; L->AddEffect(I); }
				if (!L->IsAlive()) if (AMCLiving* W = Cast<AMCLiving>(Sh)) W->Heal(5.f);
			}
		}
		World->Explode(Pos, 1.f, false, !Game || Game->Rules.bMobGriefing, this);
		break;
	}
	case EMCProjectile::ShulkerBullet:
		if (E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("mob_projectile")), 4.f))
		{
			if (AMCLiving* L = Cast<AMCLiving>(E)) { FMCEffectInstance I; I.Effect = EMCEffect::Levitation; I.Duration = 200; L->AddEffect(I); }
		}
		World->PlaySound(TEXT("shulker_bullet_hit"), Pos, 1.f, 1.f);
		break;
	case EMCProjectile::LlamaSpit:
		E->Hurt(FMCDamage::Projectile(this, Sh, TEXT("spit")), 1.f);
		break;
	case EMCProjectile::FishingBobber:
		HomingTarget = E;
		return;
	default: break;
	}
	Discard();
}

void AMCProjectile::OnPlayerTouch(AMCPlayer* Player)
{
	if (!Player || bRemoved) return;
	const bool bCanPick = (bInGround || bReturning) && (Type == EMCProjectile::Arrow || Type == EMCProjectile::SpectralArrow || Type == EMCProjectile::Trident);
	if (!bCanPick || Shake > 0) return;
	if (Type == EMCProjectile::Trident && Shooter.Get() != Player && Loyalty > 0) return;
	if (bPickup)
	{
		FMCItemStack S = Item.IsEmpty() ? FMCItemStack::Of(TEXT("arrow"), 1) : Item.Copy();
		S.Count = 1;
		if (!Player->AddItem(S)) return;
	}
	else if (!bCreativePickup && Type != EMCProjectile::Trident) return;
	Player->PlaySound(TEXT("item_pickup"), 0.2f, (Rand().NextFloat() - Rand().NextFloat()) * 1.4f + 2.f);
	Discard();
}

bool AMCProjectile::Hurt(const FMCDamage& D, float Amount)
{
	if (!IsPickable() || bRemoved) return false;
	if (Type == EMCProjectile::ShulkerBullet || Type == EMCProjectile::WindCharge) { if (World) World->SpawnParticles(TEXT("crit"), Pos, 8, 0.2f); Discard(); return true; }
	// deflect fireballs back where the attacker looks
	if (AMCEntity* A = D.Attacker.Get())
	{
		const FVector Look = A->GetLookDir();
		Vel = Look * FMath::Max(1.0, Vel.Size());
		Accel = Look * 0.1;
		Shooter = A;
		return true;
	}
	return false;
}

void AMCProjectile::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	const float Y = FMath::Lerp(PrevYaw, PrevYaw + FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Alpha);
	const float Pt = FMath::Lerp(PrevPitch, Pitch, Alpha);
	const bool bOriented = Type == EMCProjectile::Arrow || Type == EMCProjectile::SpectralArrow || Type == EMCProjectile::Trident || Type == EMCProjectile::Spear;
	if (bOriented)
	{
		float ShakeRoll = Shake > 0 ? -FMath::Sin((Shake - Alpha) * 3.f) * (Shake - Alpha) : 0.f;
		// icon art points up-right: rotate so the tip leads the flight direction
		SetActorRotation(FRotator(Pt, Y, 0.f));
		Visual->SetRelativeRotation(FRotator(0.f, 0.f, 0.f) + FRotator(-45.f + ShakeRoll, -90.f, 0.f));
	}
	else
	{
		// billboards spin slowly
		SetActorRotation(FRotator(0.f, (Age + Alpha) * 12.f, 0.f));
	}
}

void AMCProjectile::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	uint8 T = (uint8)Type; Ar << T; if (Ar.IsLoading()) Type = (EMCProjectile)T;
	Ar << Damage << bCritical << bInGround << bPickup << Knockback << Pierce << LifeInGround << bFlame << bDangerous << Loyalty << Item << Accel;
	if (Ar.IsLoading()) InitEntity();
}
