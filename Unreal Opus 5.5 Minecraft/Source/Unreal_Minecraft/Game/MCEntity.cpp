// Base entity: fixed-step simulation, Minecraft-style swept AABB collision with step-up, fluids, fire, portals.
#include "Game/MCEntity.h"
#include "Game/MCLiving.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Components/SceneComponent.h"

// ---------------------------------------------------------------------------------------------------------------------
// Damage

FMCDamage FMCDamage::Of(FName InType)
{
	FMCDamage D;
	D.Type = InType;
	const FString T = InType.ToString();
	D.bFire = T == TEXT("on_fire") || T == TEXT("in_fire") || T == TEXT("lava") || T == TEXT("hot_floor") || T == TEXT("fireball");
	D.bFall = T == TEXT("fall") || T == TEXT("stalagmite") || T == TEXT("fly_into_wall");
	D.bBypassArmor = D.bFall || T == TEXT("on_fire") || T == TEXT("drown") || T == TEXT("starve") || T == TEXT("out_of_world") || T == TEXT("generic_kill")
		|| T == TEXT("magic") || T == TEXT("wither") || T == TEXT("in_wall") || T == TEXT("freeze") || T == TEXT("cramming") || T == TEXT("dragon_breath");
	D.bMagic = T == TEXT("magic") || T == TEXT("indirect_magic") || T == TEXT("wither") || T == TEXT("dragon_breath");
	D.bBypassInvulnerability = T == TEXT("out_of_world") || T == TEXT("generic_kill");
	D.bBypassCreative = D.bBypassInvulnerability;
	D.bNoKnockback = !(T == TEXT("mob_attack") || T == TEXT("player_attack") || T == TEXT("arrow") || T == TEXT("explosion") || T == TEXT("thorns"));
	return D;
}

FMCDamage FMCDamage::Mob(AMCEntity* Attacker)
{
	FMCDamage D = Of(TEXT("mob_attack"));
	D.Attacker = Attacker;
	D.Direct = Attacker;
	D.bNoKnockback = false;
	D.bScalesWithDifficulty = true;
	if (Attacker) { D.SourcePos = Attacker->Pos; D.bHasSourcePos = true; }
	return D;
}

FMCDamage FMCDamage::PlayerAttack(AMCPlayer* P)
{
	FMCDamage D = Of(TEXT("player_attack"));
	D.Attacker = P;
	D.Direct = P;
	D.bNoKnockback = false;
	if (P) { D.SourcePos = P->Pos; D.bHasSourcePos = true; }
	return D;
}

FMCDamage FMCDamage::Projectile(AMCEntity* Proj, AMCEntity* Shooter, FName InType)
{
	FMCDamage D = Of(InType);
	D.Attacker = Shooter ? Shooter : Proj;
	D.Direct = Proj;
	D.bProjectile = true;
	D.bNoKnockback = false;
	D.bScalesWithDifficulty = Shooter && !Shooter->IsA<AMCPlayer>();
	if (Proj) { D.SourcePos = Proj->Pos; D.bHasSourcePos = true; }
	if (InType == TEXT("fireball")) D.bFire = true;
	return D;
}

FMCDamage FMCDamage::Explosion(const FVector& Pos, AMCEntity* Source)
{
	FMCDamage D = Of(TEXT("explosion"));
	D.bExplosion = true;
	D.bNoKnockback = true; // explosion knockback is applied separately
	D.SourcePos = Pos;
	D.bHasSourcePos = true;
	D.Direct = Source;
	D.Attacker = Source;
	D.bScalesWithDifficulty = true;
	return D;
}

FString FMCDamage::DeathMessage(const FString& Victim) const
{
	const FString Killer = Attacker.IsValid() ? Attacker->GetDisplayName() : FString();
	const FString T = Type.ToString();
	if (T == TEXT("fall")) return Victim + TEXT(" hit the ground too hard");
	if (T == TEXT("lava")) return Victim + TEXT(" tried to swim in lava");
	if (T == TEXT("on_fire") || T == TEXT("in_fire")) return Victim + (Killer.IsEmpty() ? TEXT(" burned to death") : TEXT(" was burned to a crisp whilst fighting ") + Killer);
	if (T == TEXT("drown")) return Victim + TEXT(" drowned");
	if (T == TEXT("starve")) return Victim + TEXT(" starved to death");
	if (T == TEXT("out_of_world")) return Victim + TEXT(" fell out of the world");
	if (T == TEXT("generic_kill")) return Victim + TEXT(" was killed");
	if (T == TEXT("cactus")) return Victim + TEXT(" was pricked to death");
	if (T == TEXT("sweet_berry_bush")) return Victim + TEXT(" was poked to death by a sweet berry bush");
	if (T == TEXT("in_wall")) return Victim + TEXT(" suffocated in a wall");
	if (T == TEXT("freeze")) return Victim + TEXT(" froze to death");
	if (T == TEXT("hot_floor")) return Victim + TEXT(" discovered the floor was lava");
	if (T == TEXT("lightning_bolt")) return Victim + TEXT(" was struck by lightning");
	if (T == TEXT("magic") || T == TEXT("indirect_magic")) return Victim + (Killer.IsEmpty() ? TEXT(" was killed by magic") : TEXT(" was killed by ") + Killer + TEXT(" using magic"));
	if (T == TEXT("wither")) return Victim + TEXT(" withered away");
	if (T == TEXT("dragon_breath")) return Victim + TEXT(" was roasted in dragon's breath");
	if (T == TEXT("stalagmite")) return Victim + TEXT(" was impaled on a stalagmite");
	if (T == TEXT("falling_block") || T == TEXT("falling_anvil")) return Victim + TEXT(" was squashed by a falling block");
	if (T == TEXT("sonic_boom")) return Victim + TEXT(" was obliterated by a sonically-charged shriek");
	if (T == TEXT("fly_into_wall")) return Victim + TEXT(" experienced kinetic energy");
	if (bExplosion) return Victim + (Killer.IsEmpty() ? TEXT(" blew up") : TEXT(" was blown up by ") + Killer);
	if (T == TEXT("arrow")) return Victim + TEXT(" was shot by ") + (Killer.IsEmpty() ? FString(TEXT("an arrow")) : Killer);
	if (T == TEXT("trident")) return Victim + TEXT(" was impaled by ") + Killer;
	if (T == TEXT("fireball")) return Victim + TEXT(" was fireballed by ") + Killer;
	if (T == TEXT("thorns")) return Victim + TEXT(" was killed while trying to hurt ") + Killer;
	if (!Killer.IsEmpty()) return Victim + TEXT(" was slain by ") + Killer;
	return Victim + TEXT(" died");
}

// ---------------------------------------------------------------------------------------------------------------------

AMCEntity::AMCEntity()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	VisualRoot->SetupAttachment(Root);
	SetCanBeDamaged(false);
}

FString AMCEntity::GetDisplayName() const
{
	if (!CustomName.IsEmpty()) return CustomName;
	FString N = TypeId.IsNone() ? GetClass()->GetName().Mid(3) : TypeId.ToString();
	N.ReplaceInline(TEXT("_"), TEXT(" "));
	if (N.Len() > 0) N[0] = FChar::ToUpper(N[0]);
	for (int32 i = 1; i < N.Len(); ++i) if (N[i - 1] == TEXT(' ')) N[i] = FChar::ToUpper(N[i]);
	return N;
}

FVector AMCEntity::GetLookDir() const
{
	const double Yr = FMath::DegreesToRadians((double)Yaw), Pr = FMath::DegreesToRadians((double)Pitch);
	return FVector(FMath::Cos(Pr) * FMath::Cos(Yr), FMath::Cos(Pr) * FMath::Sin(Yr), FMath::Sin(Pr));
}

FMCBox AMCEntity::GetBox() const { return GetBoxAt(Pos); }

FMCBox AMCEntity::GetBoxAt(const FVector& P) const
{
	const double H = Width * 0.5;
	return FMCBox(P.X - H, P.Y - H, P.Z, P.X + H, P.Y + H, P.Z + Height);
}

void AMCEntity::SetPosition(const FVector& P, bool bResetInterpolation)
{
	Pos = P;
	if (bResetInterpolation) { PrevPos = P; PrevYaw = Yaw; PrevPitch = Pitch; }
	SetActorLocation(P * MC::BlockSize, false, nullptr, ETeleportType::TeleportPhysics);
}

FMCRandom& AMCEntity::Rand() const
{
	static FMCRandom Fallback(1234);
	return World ? World->Rand : Fallback;
}

void AMCEntity::PlaySound(FName Sound, float Volume, float PitchMul) const
{
	if (bSilent || !World) return;
	World->PlaySound(Sound, Pos + FVector(0, 0, Height * 0.5), Volume, PitchMul);
}

float AMCEntity::GetBrightness() const
{
	if (!World) return 1.f;
	const FMCBlockPos P(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z + EyeHeight));
	const int32 Darken = Game ? Game->GetSkyDarken() : 0;
	return World->GetLight(P, Darken) / 15.f;
}

void AMCEntity::GetModelLight(float& OutFill, float& OutTorch) const
{
	OutFill = 0.1f;
	OutTorch = 0.f;
	if (!World) return;
	const FMCBlockPos P(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z + Height * 0.5));
	const float Blk = World->GetBlockLight(P) / 15.f;
	OutTorch = Blk * Blk * 0.45f;
	if (World->Dim == EMCDimension::Overworld)
	{
		// sky exposure times daylight (sky darken 0 at noon .. 11 at midnight), plus a faint floor like the terrain's
		const float Sky = World->GetSkyLight(P) / 15.f;
		const float Day = 1.f - (Game ? Game->GetSkyDarken() : 0) / 11.f;
		OutFill = 0.012f + Sky * FMath::Lerp(0.02f, 0.15f, Day);
	}
	else OutFill = World->Dim == EMCDimension::Nether ? 0.07f : 0.06f;   // no sky light: fixed dimension ambient
}

float AMCEntity::DistanceTo(const AMCEntity* Other) const
{
	return Other ? (float)FVector::Dist(Pos, Other->Pos) : 1e9f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle

void AMCEntity::TickEntity()
{
	PrevPos = Pos;
	PrevYaw = Yaw;
	PrevPitch = Pitch;
	++Age;
	if (PortalCooldown > 0) --PortalCooldown;
	if (!World) return;

	UpdateFluidState();
	HandleFireDamage();

	// void
	if (Pos.Z < MC::MinZ - 64)
	{
		Hurt(FMCDamage::Of(TEXT("out_of_world")), 4.f);
		if (!IsLiving()) Discard();
	}
	// powder snow freezing is accumulated by the block itself; thaw outside
	if (!bInPowderSnow && FreezeTicks > 0) FreezeTicks = FMath::Max(0, FreezeTicks - 2);
	bInPowderSnow = false;
}

void AMCEntity::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	const float Y = FMath::Lerp(PrevYaw, PrevYaw + FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Alpha);
	SetActorRotation(FRotator(0.f, Y, 0.f));
}

void AMCEntity::Discard()
{
	if (bRemoved) return;
	bRemoved = true;
	StopRiding();
	for (TWeakObjectPtr<AMCEntity>& P : Passengers)
	{
		if (P.IsValid()) { P->Vehicle.Reset(); }
	}
	Passengers.Reset();
	SetActorHiddenInGame(true);
}

void AMCEntity::Serialize(FArchive& Ar)
{
	int32 Version = 1;
	Ar << Version;
	Ar << Pos << Vel << Yaw << Pitch;
	Ar << FireTicks << FallDistance << Age << FreezeTicks << PortalCooldown;
	Ar << bOnGround << bPersistent << bInvulnerable << bSilent << bNoGravity << bGlowing;
	Ar << CustomName;
	if (Ar.IsLoading()) { PrevPos = Pos; PrevYaw = Yaw; PrevPitch = Pitch; }
}

// ---------------------------------------------------------------------------------------------------------------------
// Physics

void AMCEntity::ApplyGravityAndDrag()
{
	if (!bNoGravity) Vel.Z -= Gravity;
	Vel *= AirDrag;
	if (bOnGround)
	{
		Vel.X *= 0.5; Vel.Y *= 0.5;
	}
}

void AMCEntity::Move(const FVector& DeltaIn, bool bPreventEdgeFall)
{
	if (bNoPhysics || !World)
	{
		Pos += DeltaIn;
		return;
	}
	FVector Delta = DeltaIn;
	if (StuckSpeedMultiplier != FVector::OneVector && StuckSpeedMultiplier.SizeSquared() > 1e-7)
	{
		Delta *= StuckSpeedMultiplier;
		StuckSpeedMultiplier = FVector::OneVector;
		Vel = FVector::ZeroVector;
	}

	FMCBox Box = GetBox();

	// sneaking players do not walk off edges (Minecraft maybeBackOffFromEdge)
	if (bPreventEdgeFall && bOnGround && Delta.Z <= 0.0)
	{
		const double StepDown = -StepHeight;
		auto Supported = [&](double DX, double DY)
		{
			const FMCBox Test = Box.Offset(DX, DY, StepDown);
			return !World->IsRegionFree(Test);
		};
		double DX = Delta.X, DY = Delta.Y;
		const double Inc = 0.05;
		while (DX != 0.0 && !Supported(DX, 0.0)) { if (FMath::Abs(DX) < Inc) DX = 0.0; else DX -= Inc * FMath::Sign(DX); }
		while (DY != 0.0 && !Supported(0.0, DY)) { if (FMath::Abs(DY) < Inc) DY = 0.0; else DY -= Inc * FMath::Sign(DY); }
		while (DX != 0.0 && DY != 0.0 && !Supported(DX, DY))
		{
			if (FMath::Abs(DX) < Inc) DX = 0.0; else DX -= Inc * FMath::Sign(DX);
			if (FMath::Abs(DY) < Inc) DY = 0.0; else DY -= Inc * FMath::Sign(DY);
		}
		Delta.X = DX; Delta.Y = DY;
	}

	TArray<FMCBox> Boxes;
	World->GetCollisionBoxes(Box.Expand(Delta).Inflate(0.001), Boxes);
	// solid entities (boats, minecarts, shulkers)
	if (IsLiving() || IsA<AMCEntity>())
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(Box.Expand(Delta).Inflate(0.5), Near, this);
		for (AMCEntity* O : Near)
		{
			if (O->CanCollideWith(this) && O != Vehicle.Get() && !Passengers.Contains(O) && O->Vehicle.Get() != this) Boxes.Add(O->GetBox());
		}
	}

	auto Collide = [&](FMCBox B, const FVector& D, FVector& Out)
	{
		double DZ = D.Z;
		for (const FMCBox& C : Boxes) DZ = C.ClipZ(B, DZ);
		B = B.Offset(0, 0, DZ);
		double DX = D.X;
		double DY = D.Y;
		const bool bXFirst = FMath::Abs(DX) >= FMath::Abs(DY);
		if (bXFirst)
		{
			for (const FMCBox& C : Boxes) DX = C.ClipX(B, DX);
			B = B.Offset(DX, 0, 0);
			for (const FMCBox& C : Boxes) DY = C.ClipY(B, DY);
			B = B.Offset(0, DY, 0);
		}
		else
		{
			for (const FMCBox& C : Boxes) DY = C.ClipY(B, DY);
			B = B.Offset(0, DY, 0);
			for (const FMCBox& C : Boxes) DX = C.ClipX(B, DX);
			B = B.Offset(DX, 0, 0);
		}
		Out = FVector(DX, DY, DZ);
		return B;
	};

	FVector Result;
	FMCBox NewBox = Collide(Box, Delta, Result);

	// step up (stairs, slabs, path blocks)
	const bool bHorizontalBlocked = Result.X != Delta.X || Result.Y != Delta.Y;
	const bool bWasGrounded = bOnGround || (Result.Z != Delta.Z && Delta.Z < 0.0);
	if (StepHeight > 0.f && bWasGrounded && bHorizontalBlocked)
	{
		TArray<FMCBox> StepBoxes;
		World->GetCollisionBoxes(Box.Expand(FVector(Delta.X, Delta.Y, StepHeight)).Inflate(0.001), StepBoxes);
		Boxes.Append(StepBoxes);
		FVector StepRes;
		FMCBox StepBox = Collide(Box, FVector(Delta.X, Delta.Y, StepHeight), StepRes);
		// then settle down
		double Down = -(StepRes.Z) + FMath::Min(Delta.Z, 0.0);
		for (const FMCBox& C : Boxes) Down = C.ClipZ(StepBox, Down);
		StepBox = StepBox.Offset(0, 0, Down);
		StepRes.Z += Down;
		const double HStep = StepRes.X * StepRes.X + StepRes.Y * StepRes.Y;
		const double HNorm = Result.X * Result.X + Result.Y * Result.Y;
		if (HStep > HNorm + 1e-7)
		{
			Result = StepRes;
			NewBox = StepBox;
		}
	}

	bHorizontalCollision = Result.X != Delta.X || Result.Y != Delta.Y;
	bVerticalCollision = Result.Z != Delta.Z;
	bCollidedBelow = bVerticalCollision && Delta.Z < 0.0;
	const bool bWasOnGround = bOnGround;
	bOnGround = bCollidedBelow;
	(void)bWasOnGround;

	Pos = FVector((NewBox.Min.X + NewBox.Max.X) * 0.5, (NewBox.Min.Y + NewBox.Max.Y) * 0.5, NewBox.Min.Z);

	CheckFallDamage(Result.Z, bOnGround);

	if (Result.X != Delta.X) Vel.X = 0.0;
	if (Result.Y != Delta.Y) Vel.Y = 0.0;
	if (Result.Z != Delta.Z)
	{
		// bouncy / sticky blocks decide the vertical reaction when landing
		const FMCState Below = World->GetState(BlockBelow());
		const FName BN = FMCBlocks::GetByState(Below).Name;
		if (Delta.Z < 0.0 && BN == TEXT("slime_block") && !bSneaking)
		{
			Vel.Z = -Vel.Z * (IsLiving() ? 1.0 : 0.8);
		}
		else if (Delta.Z < 0.0 && FMCBlocks::GetByState(Below).Model == EMCModel::Bed && !bSneaking)
		{
			Vel.Z = -Vel.Z * 0.66;
		}
		else
		{
			Vel.Z = 0.0;
		}
	}

	// block speed factors (soul sand, honey)
	const FMCState Under = World->GetState(FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z - 0.5000001)));
	const FMCState AtFeet = World->GetState(BlockPos());
	float Factor = FMCBlocks::GetByState(AtFeet).SpeedFactor;
	if (FMath::IsNearlyEqual(Factor, 1.f)) Factor = FMCBlocks::GetByState(Under).SpeedFactor;
	BlockSpeedFactor = Factor;

	UpdateInsideBlocks();
}

void AMCEntity::CheckFallDamage(double DeltaZ, bool bGroundNow)
{
	if (bInWater) { FallDistance = 0.f; return; }
	if (bGroundNow)
	{
		if (FallDistance > 0.f && World)
		{
			const FMCBlockPos BP = BlockBelow();
			const FMCState S = World->GetState(BP);
			OnFellOnGround(FallDistance, S);
			if (S != 0) FMCBlocks::GetByState(S).Behavior->OnFallOn(*World, BP, S, this, FallDistance);
		}
		FallDistance = 0.f;
	}
	else if (DeltaZ < 0.0)
	{
		FallDistance -= (float)DeltaZ;
	}
}

void AMCEntity::UpdateFluidState()
{
	if (!World) return;
	bWasInWater = bInWater;
	const FMCBox B = GetBox().Inflate(-0.001);
	double Surface = 0.0;
	bInWater = World->IsInFluid(B, FMCBlocks::C.WaterId, &Surface);
	bInLava = World->IsInFluid(B, FMCBlocks::C.LavaId);
	const FVector Eye = GetEyePos();
	const FMCBlockPos EP(MC::FloorToInt(Eye.X), MC::FloorToInt(Eye.Y), MC::FloorToInt(Eye.Z));
	const FMCState ES = World->GetState(EP);
	const FMCStateInfo& EI = FMCBlocks::Info(ES);
	const bool bEyeWater = EI.Block == FMCBlocks::C.WaterId || (EI.Flags & MCB_Waterlogged);
	bEyesInWater = bEyeWater && Eye.Z < EP.Z + World->GetFluidHeight(EP) + 0.0;
	if (bInWater) { FallDistance = 0.f; if (FireTicks > 0 && !bInLava) Extinguish(); }
}

void AMCEntity::UpdateInsideBlocks()
{
	if (!World) return;
	const FMCBox B = GetBox().Inflate(-0.001);
	const int32 X0 = MC::FloorToInt(B.Min.X), X1 = MC::FloorToInt(B.Max.X);
	const int32 Y0 = MC::FloorToInt(B.Min.Y), Y1 = MC::FloorToInt(B.Max.Y);
	const int32 Z0 = MC::FloorToInt(B.Min.Z), Z1 = MC::FloorToInt(B.Max.Z);
	const int32 PrevPortal = PortalTime;
	bool bPortalHandled = false;
	bInsidePortal = false;
	for (int32 X = X0; X <= X1; ++X)
		for (int32 Y = Y0; Y <= Y1; ++Y)
			for (int32 Z = Z0; Z <= Z1; ++Z)
			{
				const FMCBlockPos P(X, Y, Z);
				const FMCState S = World->GetState(P);
				if (S == 0) continue;
				const FMCStateInfo& I = FMCBlocks::Info(S);
				if (I.Flags & MCB_Portal)
				{
					bInsidePortal = true;
					if (bPortalHandled) continue;
					bPortalHandled = true;
					// creative players travel instantly
					const AMCPlayer* PL = Cast<AMCPlayer>(this);
					if (PL && PL->IsCreative() && I.Block == FMCBlocks::C.NetherPortalId && PortalCooldown == 0) PortalTime = FMath::Max(PortalTime, 79);
				}
				FMCBlocks::Get(I.Block).Behavior->OnEntityInside(*World, P, S, this);
				if (bRemoved || !World) return;
			}
	if (!bInsidePortal && PortalTime > 0 && PortalTime == PrevPortal) PortalTime = FMath::Max(0, PortalTime - 4);

	// standing on a block
	if (bOnGround && World)
	{
		const FMCBlockPos BP = BlockBelow();
		const FMCState S = World->GetState(BP);
		if (S != 0) FMCBlocks::GetByState(S).Behavior->OnSteppedOn(*World, BP, S, this);
	}
}

void AMCEntity::HandleFireDamage()
{
	if (bInLava && !bFireImmune)
	{
		SetOnFire(15);
		if (Age - LastFireDamageTick >= 10 || LastFireDamageTick == 0)
		{
			LastFireDamageTick = Age;
			Hurt(FMCDamage::Of(TEXT("lava")), 4.f);
		}
	}
	if (FireTicks > 0)
	{
		if (bFireImmune)
		{
			FireTicks = FMath::Max(0, FireTicks - 4);
		}
		else
		{
			if (FireTicks % 20 == 0) Hurt(FMCDamage::Of(TEXT("on_fire")), 1.f);
			--FireTicks;
		}
	}
}

void AMCEntity::SetOnFire(int32 Seconds)
{
	int32 Ticks = Seconds * MC::TicksPerSecond;
	if (const AMCLiving* L = Cast<AMCLiving>(this))
	{
		const int32 FP = L->GetEnchantMax(EMCEnchant::FireProtection);
		if (FP > 0) Ticks -= FMath::FloorToInt(Ticks * FP * 0.15f);
	}
	if (FireTicks < Ticks) FireTicks = Ticks;
}

void AMCEntity::PushAwayFrom(AMCEntity* Other)
{
	if (!Other || Other == this || IsPassenger() || Other->IsPassenger()) return;
	double DX = Other->Pos.X - Pos.X, DY = Other->Pos.Y - Pos.Y;
	double D = FMath::Max(FMath::Abs(DX), FMath::Abs(DY));
	if (D < 0.01) return;
	D = FMath::Sqrt(D);
	DX /= D; DY /= D;
	const double Inv = FMath::Min(1.0, 1.0 / D);
	DX *= Inv * 0.05; DY *= Inv * 0.05;
	if (!IsPassenger() && IsPushable()) { Vel.X -= DX; Vel.Y -= DY; }
	if (!Other->IsPassenger() && Other->IsPushable()) { Other->Vel.X += DX; Other->Vel.Y += DY; }
}

void AMCEntity::StartRiding(AMCEntity* V)
{
	if (!V || V == this) return;
	StopRiding();
	Vehicle = V;
	V->Passengers.AddUnique(this);
	Vel = FVector::ZeroVector;
	FallDistance = 0.f;
}

void AMCEntity::StopRiding()
{
	if (AMCEntity* V = Vehicle.Get())
	{
		V->Passengers.Remove(this);
		// dismount next to the vehicle
		const FVector Off = V->GetPassengerOffset(this);
		FVector Target = V->Pos + FVector(0, 0, FMath::Max(0.0, Off.Z)) + FVector(0, 0, 0.1);
		if (World)
		{
			static const FVector Tries[] = { FVector(0, 0, 0), FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& T : Tries)
			{
				const FVector Cand = V->Pos + T * (V->Width * 0.5 + Width * 0.5 + 0.1) + FVector(0, 0, T.IsZero() ? V->Height : 0.05);
				if (World->IsRegionFree(GetBoxAt(Cand))) { Target = Cand; break; }
			}
		}
		Vehicle.Reset();
		SetPosition(Target, false);
	}
}

void AMCEntity::TeleportTo(const FVector& P)
{
	StopRiding();
	SetPosition(P, true);
	Vel = FVector::ZeroVector;
	FallDistance = 0.f;
}
