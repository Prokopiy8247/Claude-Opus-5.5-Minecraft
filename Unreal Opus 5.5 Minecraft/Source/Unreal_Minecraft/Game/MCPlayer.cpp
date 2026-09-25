// Player core: movement states, camera, inventory, survival stats, death & respawn, menus, persistence.
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "Game/MCHeldItem.h"
#include "Render/MCRig.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Camera/CameraComponent.h"

AMCPlayer::AMCPlayer()
{
	Kind = EMCEntityKind::Player;
	TypeId = TEXT("player");
	Width = 0.6f; Height = 1.8f; EyeHeight = 1.62f;
	MaxHealth = 20.f; Health = 20.f;
	MoveSpeed = 0.1f;
	FlyingSpeed = 0.02f;
	bPersistent = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetRelativeLocation(FVector(0, 0, 162));
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(70.f);
	// Minecraft's FOV option is vertical (70 = roughly 102 degrees across a 16:9 screen)
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
	Camera->PostProcessBlendWeight = 1.f;

	HeldItem = CreateDefaultSubobject<UMCHeldItemComponent>(TEXT("HeldItem"));
	HeldItem->SetupAttachment(Camera);

	Body = CreateDefaultSubobject<UMCRigComponent>(TEXT("Body"));
	Body->SetupAttachment(VisualRoot);

	Inventory.Init(MCInv::Size);
	EnderInventory.Init(27);
}

void AMCPlayer::InitEntity()
{
	Super::InitEntity();
	Body->SetRig(TEXT("player"), FColor(255, 255, 255));
	InvMenu = MakeShared<FMCInventoryMenu>(this);
	SetGameMode(GameMode);
	ViewYaw = Yaw; ViewPitch = Pitch;
}

// ---------------------------------------------------------------------------------------------------------------------
// Modes

void AMCPlayer::SetGameMode(EMCGameMode M)
{
	GameMode = M;
	bInstabuild = M == EMCGameMode::Creative;
	bMayFly = M == EMCGameMode::Creative || M == EMCGameMode::Spectator;
	if (!bMayFly) bFlying = false;
	if (M == EMCGameMode::Spectator) { bFlying = true; bNoPhysics = true; }
	else bNoPhysics = false;
	bInvulnerable = M == EMCGameMode::Creative || M == EMCGameMode::Spectator;
	if (M != EMCGameMode::Creative && M != EMCGameMode::Spectator) bInvulnerable = false;
	StopMining();
}

// ---------------------------------------------------------------------------------------------------------------------
// Equipment mapping onto the inventory

FMCItemStack& AMCPlayer::GetItem(EMCEquipSlot S)
{
	switch (S)
	{
	case EMCEquipSlot::MainHand: return Inventory.Slots[Selected];
	case EMCEquipSlot::OffHand: return Inventory.Slots[MCInv::Offhand];
	case EMCEquipSlot::Head: return Inventory.Slots[39];
	case EMCEquipSlot::Chest: return Inventory.Slots[38];
	case EMCEquipSlot::Legs: return Inventory.Slots[37];
	case EMCEquipSlot::Feet: return Inventory.Slots[36];
	default: return Inventory.Slots[Selected];
	}
}

const FMCItemStack& AMCPlayer::GetItem(EMCEquipSlot S) const
{
	return const_cast<AMCPlayer*>(this)->GetItem(S);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick

void AMCPlayer::TickEntity()
{
	bNoPhysics = IsSpectator();
	Yaw = ViewYaw;
	Pitch = FMath::Clamp(ViewPitch, -90.f, 90.f);
	PrevEyeHeight = EyeHeightCurrent;
	PrevBobAmount = BobAmount;
	PrevPortalOverlay = PortalOverlay;
	if (UseDelay > 0) --UseDelay;
	++AttackStrengthTicker;

	if (DeathTime > 0 || Health <= 0.f)
	{
		Super::TickEntity();
		++TicksSinceDeath;
		if (Game && Game->Rules.bDoImmediateRespawn && TicksSinceDeath > 2) Respawn();
		return;
	}
	TicksSinceDeath = 0;

	if (bSleeping)
	{
		++SleepCounter;
		Vel = FVector::ZeroVector;
		if (World)
		{
			const FMCState BS = World->GetState(BedPos);
			if (FMCBlocks::GetByState(BS).Model != EMCModel::Bed) WakeUp(false);
		}
		if (Game && Game->IsDay() && SleepCounter > 100 && !Game->bThundering) WakeUp(true);
	}

	Super::TickEntity();
	if (bRemoved) return;

	TickMovementStates();
	TickCooldowns();
	TickPickup();
	TickMining();
	TickUseItem();
	if (IsSurvivalLike()) TickFood();
	HandlePortalTransitions();

	// footsteps
	const double DX = Pos.X - PrevPos.X, DY = Pos.Y - PrevPos.Y;
	const double HDist = FMath::Sqrt(DX * DX + DY * DY);
	if (bOnGround && !bFlying && HDist > 0.001 && !IsSpectator())
	{
		StepDistance += HDist * (bSprinting ? 0.8 : 0.6);
		StatDistanceWalked += HDist;
		if (StepDistance > NextStepSound)
		{
			NextStepSound = StepDistance + 1.0;
			const FMCState Below = World->GetState(BlockBelow());
			if (Below && !bSneaking) World->PlayBlockSound(Below, 2, Pos);
		}
		if (bSprinting) CauseExhaustion(0.1f * (float)HDist);
	}
	if (bInWater && HDist > 0.001) CauseExhaustion(0.01f * (float)HDist);
	if (bInWater && !bWasInWater && Vel.Z < -0.2) { PlaySound(TEXT("splash"), 0.4f, 1.f); World->SpawnParticles(TEXT("splash"), Pos, 16, 0.4f, FVector(0, 0, 0.3), FColor(180, 200, 255)); }

	// validate open menu
	if (Menu.IsValid())
	{
		Menu->Tick();
		if (!Menu->StillValid()) CloseMenu();
	}
	EquipAnim = FMath::Min(1.f, EquipAnim + 0.25f);
	if (!(LastHeldForAnim.Id == HeldConst().Id && LastHeldForAnim.Count == HeldConst().Count)) { LastHeldForAnim = HeldConst().Copy(); }
}

void AMCPlayer::AIStep()
{
	float F = Input.Forward, S = Input.Strafe;
	const bool bWantSneak = Input.bSneak && !bFlying;
	if (bSneaking != bWantSneak)
	{
		// can only stand up if there is room
		if (!bWantSneak && World && !bNoPhysics)
		{
			const FMCBox Stand(Pos.X - Width * 0.5, Pos.Y - Width * 0.5, Pos.Z, Pos.X + Width * 0.5, Pos.Y + Width * 0.5, Pos.Z + 1.8);
			if (World->IsRegionFree(Stand)) bSneaking = false;
		}
		else bSneaking = bWantSneak;
	}
	Height = bSneaking ? 1.5f : 1.8f;
	if (bSneaking)
	{
		const float SwiftSneak = 0.3f + 0.15f * FMath::Min(3, GetEnchantMax(EMCEnchant::SwiftSneak));
		F *= SwiftSneak; S *= SwiftSneak;
	}
	if (bUsingItem && !IsPassenger()) { F *= 0.2f; S *= 0.2f; }

	// sprinting
	const bool bCanSprint = (FoodLevel > 6 || bMayFly) && !bUsingItem && !HasEffect(EMCEffect::Blindness) && !bSneaking;
	if (Input.bForwardPressed)
	{
		if (SprintTriggerTime > 0 && bCanSprint) bSprinting = true;
		SprintTriggerTime = 7;
	}
	if (SprintTriggerTime > 0) --SprintTriggerTime;
	if (Input.bSprint && bCanSprint && F > 0.8f * (bSneaking ? 0.3f : 1.f)) bSprinting = true;
	if (bSprinting && (F < 0.8f * (bSneaking ? 0.3f : 1.f) || !bCanSprint || (bHorizontalCollision && !bInWater) || (bInWater && !bEyesInWater && !bFlying && false)))
	{
		bSprinting = false;
	}

	// double tap jump toggles flight
	if (Input.bJumpPressed && bMayFly && !IsSpectator())
	{
		if (JumpTriggerTime > 0)
		{
			bFlying = !bFlying;
			JumpTriggerTime = 0;
			if (bFlying) Vel.Z = FMath::Max(Vel.Z, 0.0);
		}
		else JumpTriggerTime = 7;
	}
	if (JumpTriggerTime > 0) --JumpTriggerTime;
	if (IsSpectator()) bFlying = true;

	// elytra: jump while falling
	const FMCItemStack& ChestItem = Inventory.Slots[38];
	if (Input.bJumpPressed && !bOnGround && !bFlying && !bElytraFlying && !bInWater && Vel.Z < 0.0 && !ChestItem.IsEmpty() && ChestItem.Item().Name == TEXT("elytra")
		&& ChestItem.Damage < ChestItem.Item().MaxDamage - 1)
	{
		bElytraFlying = true;
	}
	if (bElytraFlying && (bOnGround || bInWater || bFlying || ChestItem.IsEmpty() || ChestItem.Item().Name != TEXT("elytra"))) bElytraFlying = false;
	if (bElytraFlying && Age % 20 == 0 && !IsCreative())
	{
		FMCItemStack& E = Inventory.Slots[38];
		if (E.Damage < E.Item().MaxDamage - 1) E.Damage += 1;
		else bElytraFlying = false;
	}

	if (bFlying && !IsPassenger())
	{
		if (Input.bSneak) Vel.Z -= FlySpeed * 3.0;
		if (Input.bJump) Vel.Z += FlySpeed * 3.0;
		if (bOnGround && !IsSpectator() && Input.bSneak) bFlying = false;
	}
	bJumping = Input.bJump && !bFlying;
	MoveForward = F;
	MoveStrafe = -S;
	FlyingSpeed = bSprinting ? 0.026f : 0.02f;
	if (IsPassenger())
	{
		// vehicles read MoveForward / MoveStrafe of their rider
		if (Input.bSneak) { StopRiding(); }
		return;
	}
	Super::AIStep();
}

void AMCPlayer::Travel(float Strafe, float Up, float Forward)
{
	if (bFlying)
	{
		const double VZ = Vel.Z;
		const float Speed = FlySpeed * (bSprinting ? 2.f : 1.f);
		FVector2D In(Forward, -Strafe);
		const double L2 = In.SizeSquared();
		if (L2 > 1e-7)
		{
			if (L2 > 1.0) In /= FMath::Sqrt(L2);
			In *= Speed;
			const double R = FMath::DegreesToRadians((double)Yaw);
			Vel.X += In.X * FMath::Cos(R) - In.Y * FMath::Sin(R);
			Vel.Y += In.X * FMath::Sin(R) + In.Y * FMath::Cos(R);
		}
		Move(Vel);
		Vel.X *= 0.91; Vel.Y *= 0.91;
		Vel.Z = VZ * 0.6;
		FallDistance = 0.f;
		if (bOnGround && !IsSpectator() && !Input.bJump) bFlying = false;
		return;
	}
	Super::Travel(Strafe, Up, Forward);
}

void AMCPlayer::Jump()
{
	Super::Jump();
	CauseExhaustion(bSprinting ? 0.2f : 0.05f);
}

float AMCPlayer::GetSpeed() const
{
	return Super::GetSpeed();
}

void AMCPlayer::TickMovementStates()
{
	const float TargetEye = bSleeping ? 0.2f : (bSneaking ? 1.27f : (bElytraFlying ? 0.4f : 1.62f));
	EyeHeightCurrent += (TargetEye - EyeHeightCurrent) * 0.5f;
	EyeHeight = TargetEye;
	// view bobbing amount (Minecraft: horizontal speed, 0 when airborne)
	const double DX = Pos.X - PrevPos.X, DY = Pos.Y - PrevPos.Y;
	float Target = (bOnGround && !bFlying) ? FMath::Min(0.1f, (float)FMath::Sqrt(DX * DX + DY * DY)) : 0.f;
	if (Health <= 0.f) Target = 0.f;
	BobAmount += (Target - BobAmount) * 0.4f;
	BobPhase += (float)FMath::Sqrt(DX * DX + DY * DY) * 0.6f;
	// FOV modifier (sprinting / flying / speed / bow draw)
	float Fov = 1.f;
	if (bFlying) Fov *= 1.1f;
	Fov *= (GetSpeed() / MoveSpeed + 1.f) / 2.f;
	if (FMath::Abs(MoveSpeed) < 1e-5f) Fov = 1.f;
	if (bUsingItem && !HeldConst().IsEmpty() && HeldConst().Item().Kind == EMCItemKind::Bow)
	{
		const float Draw = FMath::Min(1.f, (UseItemDuration - UseItemRemaining) / 20.f);
		Fov *= 1.f - Draw * Draw * 0.15f;
	}
	FOVModifier += (Fov - FOVModifier) * 0.5f;
}

void AMCPlayer::TickCooldowns()
{
	if (ItemCooldown > 0) --ItemCooldown;
	if (DestroyDelay > 0) --DestroyDelay;
}

void AMCPlayer::TickPickup()
{
	if (!World || IsSpectator()) return;
	TArray<AMCEntity*> Near;
	World->GetEntitiesInBox(GetBox().Inflate(1.0).Offset(0, 0, -0.5).Expand(FVector(0, 0, 0.5)), Near, this);
	for (AMCEntity* E : Near)
	{
		if (E && !E->bRemoved) E->OnPlayerTouch(this);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Visuals / camera

void AMCPlayer::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	SetActorRotation(FRotator::ZeroRotator);
	UpdateCamera(Alpha, DeltaSeconds);

	// body rig (third person / shadow)
	const float BYaw = FMath::Lerp(PrevBodyYaw, PrevBodyYaw + FMath::FindDeltaAngleDegrees(PrevBodyYaw, BodyYaw), Alpha);
	VisualRoot->SetWorldRotation(FRotator(0.f, BYaw, 0.f));
	FMCRigPose Pose;
	Pose.LimbSwing = LimbSwing - LimbSwingAmount * (1.f - Alpha);
	Pose.LimbAmount = FMath::Lerp(PrevLimbSwingAmount, LimbSwingAmount, Alpha);
	Pose.Age = Age + Alpha;
	Pose.HeadYaw = FMath::FindDeltaAngleDegrees(BYaw, ViewYaw);
	Pose.HeadPitch = ViewPitch;
	Pose.Attack = AttackAnim;
	Pose.bSneaking = bSneaking;
	Pose.bFlying = bElytraFlying;
	Pose.bRiding = IsPassenger();
	Pose.bSleeping = bSleeping;
	Pose.bBlocking = bBlocking;
	Pose.bHoldingItem = !HeldConst().IsEmpty();
	Pose.Hurt = HurtTime > 0 ? HurtTime / 10.f : 0.f;
	Pose.Death = DeathTime > 0 ? FMath::Min(1.f, (DeathTime + Alpha) / 20.f) : 0.f;
	Body->ApplyPose(Pose);
	Body->SetHurtFlash(Pose.Hurt > 0.f || Pose.Death > 0.f ? 0.6f : 0.f);
	float Fill, Torch;
	GetModelLight(Fill, Torch);
	Body->SetLighting(Fill, Torch);
	const bool bThirdPerson = CameraMode != 0;
	Body->SetVisibility(bThirdPerson && !IsSpectator(), true);
	for (UStaticMeshComponent* M : Body->Meshes)
	{
		if (M) { M->bCastHiddenShadow = !IsSpectator(); }
	}
	HeldItem->SetVisibility(!bThirdPerson && !IsSpectator() && Health > 0.f, true);
	HeldItem->SetStack(HeldConst());
	HeldItem->UpdatePose(this, Alpha, DeltaSeconds);
}

void AMCPlayer::UpdateCamera(float Alpha, float DeltaSeconds)
{
	const float Eye = FMath::Lerp(PrevEyeHeight, EyeHeightCurrent, Alpha);
	FVector Loc(0, 0, Eye * MC::BlockSize);
	float Roll = 0.f;
	float PitchAdd = 0.f;
	// view bobbing
	if (!bOnGround || bFlying) {}
	const float Bob = FMath::Lerp(PrevBobAmount, BobAmount, Alpha);
	const bool bBobbing = true;
	if (bBobbing && CameraMode == 0)
	{
		const float Walk = BobPhase * PI;
		const FVector Right = FRotator(0, ViewYaw, 0).RotateVector(FVector(0, 1, 0));
		Loc += Right * (FMath::Sin(Walk) * Bob * 0.5f * MC::BlockSizeF);
		Loc.Z -= FMath::Abs(FMath::Cos(Walk) * Bob) * MC::BlockSizeF;
		Roll += FMath::Sin(Walk) * Bob * 3.f;
		PitchAdd -= FMath::Abs(FMath::Cos(Walk - 0.2f) * Bob) * 5.f;
	}
	// hurt tilt
	if (HurtTime > 0 && Health > 0.f)
	{
		const float T = (HurtTime - Alpha) / 10.f;
		Roll += FMath::Sin(T * T * T * T * PI) * 14.f;
	}
	if (DeathTime > 0) Roll += FMath::Min(1.f, DeathTime / 20.f) * 40.f;
	if (bSleeping) { Loc.Z = 0.3f * MC::BlockSizeF; }
	// nausea / portal wobble
	const float Wobble = FMath::Lerp(PrevPortalOverlay, PortalOverlay, Alpha);
	if (HasEffect(EMCEffect::Nausea) || Wobble > 0.f) Roll += FMath::Sin((Age + Alpha) * 0.15f) * 4.f * FMath::Max(Wobble, HasEffect(EMCEffect::Nausea) ? 1.f : 0.f);

	FRotator Rot(ViewPitch + PitchAdd, ViewYaw, Roll);
	if (CameraMode != 0 && World)
	{
		// third person: pull the camera back, stopping at terrain
		const FVector EyeW = (InterpolatedPos(Alpha) + FVector(0, 0, Eye));
		FVector Dir = Rot.Vector();
		if (CameraMode == 2) { Dir = -Dir; Rot = FRotator(-Rot.Pitch, Rot.Yaw + 180.f, Rot.Roll); }
		double Dist = 4.0;
		FMCRayHit Hit;
		if (World->Raycast(EyeW, -Dir, 4.2, Hit, false, false)) Dist = FMath::Max(0.3, Hit.Distance - 0.25);
		Loc += -Dir * Dist * MC::BlockSize;
	}
	Camera->SetRelativeLocation(Loc);
	Camera->SetWorldRotation(Rot);
	float BaseFOV = 70.f;
	if (Game && Game->Player == this)
	{
		if (const UWorld* UW = GetWorld()) { (void)UW; }
	}
	BaseFOV = FOVOverride > 0.f ? FOVOverride : BaseFOV;
	Camera->SetFieldOfView(FMath::Clamp(BaseFOV * GetFOVMultiplier(), 5.f, 170.f));
}

float AMCPlayer::GetFOVMultiplier() const
{
	if (IsUsingSpyglass() && CameraMode == 0) return 0.1f;
	return FOVModifier * (bEyesInWater ? 0.857f : 1.f);
}

bool AMCPlayer::IsUsingSpyglass() const
{
	return bUsingItem && !HeldConst().IsEmpty() && HeldConst().Item().Kind == EMCItemKind::Spyglass;
}

// ---------------------------------------------------------------------------------------------------------------------
// Damage / death / respawn

bool AMCPlayer::Hurt(const FMCDamage& D, float Amount)
{
	if (IsSpectator() && !D.bBypassInvulnerability) return false;
	if (IsCreative() && !D.bBypassCreative) return false;
	if (Health <= 0.f) return false;
	if (bSleeping) WakeUp(false);
	if (D.bScalesWithDifficulty && Game)
	{
		switch (Game->Difficulty)
		{
		case EMCDifficulty::Peaceful: if (!D.bExplosion) Amount = 0.f; break;
		case EMCDifficulty::Easy: Amount = FMath::Min(Amount / 2.f + 1.f, Amount); break;
		case EMCDifficulty::Hard: Amount *= 1.5f; break;
		default: break;
		}
	}
	if (Amount <= 0.f) return false;
	// shield
	if (bBlocking && !D.bBypassArmor && (D.bHasSourcePos || D.Attacker.IsValid()))
	{
		const FVector Src = D.bHasSourcePos ? D.SourcePos : D.Attacker->Pos;
		FVector To = Src - Pos; To.Z = 0;
		const FVector Look = FRotator(0, Yaw, 0).Vector();
		if (!To.IsNearlyZero() && FVector::DotProduct(To.GetSafeNormal(), Look) > 0.0)
		{
			PlaySound(TEXT("shield_block"), 1.f, 0.8f + Rand().NextFloat() * 0.4f);
			if (Amount >= 3.f)
			{
				const EMCEquipSlot Hand = bUsingOffhand ? EMCEquipSlot::OffHand : EMCEquipSlot::MainHand;
				FMCItemStack& Sh = GetItem(Hand);
				if (Sh.DamageItem(1 + FMath::FloorToInt(Amount), Rand())) { PlaySound(TEXT("shield_break"), 1.f, 1.f); StopUsingItem(false); }
			}
			if (AMCLiving* A = Cast<AMCLiving>(D.Direct.Get()))
			{
				if (!D.bProjectile) A->Knockback(0.5, Pos.X - A->Pos.X, Pos.Y - A->Pos.Y);
				const FMCItemStack& W = A->MainHandConst();
				if (!W.IsEmpty() && W.Item().ToolType == EMCTool::Axe) { ItemCooldown = 100; StopUsingItem(false); PlaySound(TEXT("shield_break"), 0.8f, 0.8f); }
			}
			return false;
		}
	}
	const bool bApplied = Super::Hurt(D, Amount);
	if (bApplied)
	{
		CauseExhaustion(0.1f);
		if (Health > 0.f) PlaySound(D.bFire ? TEXT("player_hurt_on_fire") : (D.Type == TEXT("drown") ? TEXT("player_hurt_drown") : TEXT("player_hurt")), 1.f, 0.9f + Rand().NextFloat() * 0.2f);
		SinceLastHurt = 0;
	}
	return bApplied;
}

void AMCPlayer::Die(const FMCDamage& D)
{
	const bool bWasAlive = DeathTime == 0;
	Super::Die(D);
	if (!bWasAlive) return;
	StopUsingItem(false);
	StopMining();
	bFlying = false;
	bElytraFlying = false;
	if (bSleeping) WakeUp(false);
	++StatDeaths;
	bDeadScreen = true;
	PlaySound(TEXT("player_death"), 1.f, 1.f);
	if (Game) Game->AddChat(LastDeathMessage);
	const bool bKeep = Game && Game->Rules.bKeepInventory;
	if (!bKeep && World)
	{
		for (FMCItemStack& S : Inventory.Slots)
		{
			if (S.IsEmpty()) continue;
			if (S.GetEnchant(EMCEnchant::VanishingCurse) > 0) { S.Clear(); continue; }
			DropStack(S, false);
			S.Clear();
		}
		if (!CarriedStack.IsEmpty()) { DropStack(CarriedStack, false); CarriedStack.Clear(); }
		const int32 XPDrop = FMath::Min(XPLevel * 7, 100);
		if (XPDrop > 0) World->SpawnXP(Pos + FVector(0, 0, 0.5), XPDrop);
		XPLevel = 0; XPProgress = 0.f; XPTotal = 0;
	}
	CloseMenu();
	if (Game && Game->bHardcore) SetGameMode(EMCGameMode::Spectator);
}

void AMCPlayer::Respawn()
{
	if (!Game) return;
	FVector Target;
	EMCDimension Dim = EMCDimension::Overworld;
	bool bFound = false;
	if (bHasSpawnPoint)
	{
		FMCWorld* SW = Game->EnsureWorld(SpawnDim);
		if (SW)
		{
			SW->ForceLoadArea(FMCChunkPos::FromBlock(SpawnPoint), 1, 5.0);
			const FMCState S = SW->GetState(SpawnPoint);
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const bool bBed = B.Model == EMCModel::Bed;
			const bool bAnchor = B.Name == TEXT("respawn_anchor") && (FMCBlocks::MetaOf(S) & 7) > 0;
			if (bBed || bAnchor || bSpawnForced)
			{
				// free space next to or above the block
				static const FIntVector Offs[] = { FIntVector(0, 0, 1), FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0),
					FIntVector(1, 1, 0), FIntVector(-1, -1, 0), FIntVector(1, -1, 0), FIntVector(-1, 1, 0) };
				for (const FIntVector& O : Offs)
				{
					const FMCBlockPos C = SpawnPoint + O;
					const FVector Cand(C.X + 0.5, C.Y + 0.5, C.Z);
					if (SW->IsRegionFree(GetBoxAt(Cand)) && (FMCBlocks::IsSolid(SW->GetState(C.Down())) || O.Z > 0))
					{
						Target = Cand; Dim = SpawnDim; bFound = true; break;
					}
				}
				if (bFound && bAnchor)
				{
					SW->SetState(SpawnPoint, B.State((FMCBlocks::MetaOf(S) & 7) - 1), MCSet_Default);
					SW->PlaySound(TEXT("respawn_anchor_deplete"), FVector(SpawnPoint.X + 0.5, SpawnPoint.Y + 0.5, SpawnPoint.Z + 0.5));
				}
			}
		}
		if (!bFound)
		{
			SendMessage(TEXT("You have no home bed or charged respawn anchor, or it was obstructed"));
			bHasSpawnPoint = false;
		}
	}
	if (!bFound)
	{
		FMCWorld* OW = Game->EnsureWorld(EMCDimension::Overworld);
		OW->ForceLoadArea(FMCChunkPos::FromBlock(Game->WorldSpawn), 1, 10.0);
		const FMCBlockPos Safe = OW->FindSafeSpawn(Game->WorldSpawn.X, Game->WorldSpawn.Y);
		Target = FVector(Safe.X + 0.5, Safe.Y + 0.5, Safe.Z);
		Dim = EMCDimension::Overworld;
	}
	Health = GetMaxHealth();
	DeathTime = 0; HurtTime = 0; InvulnerableTime = 60;
	FoodLevel = 20; Saturation = 5.f; Exhaustion = 0.f;
	AirSupply = MaxAir; FireTicks = 0; FreezeTicks = 0; FallDistance = 0.f;
	ClearEffects();
	Vel = FVector::ZeroVector;
	bDeadScreen = false;
	bSprinting = false;
	SetActorHiddenInGame(false);
	if (Dim != Game->ActiveDim) Game->ChangeDimension(this, Dim, &Target);
	else TeleportTo(Target);
	if (IsSpectator() && !(Game->bHardcore)) SetGameMode(Game->DefaultGameMode);
}

// ---------------------------------------------------------------------------------------------------------------------
// Survival

void AMCPlayer::CauseExhaustion(float Amount)
{
	if (!IsSurvivalLike()) return;
	Exhaustion = FMath::Min(40.f, Exhaustion + Amount);
}

bool AMCPlayer::CanEat(bool bAlwaysEdible) const
{
	return bAlwaysEdible || FoodLevel < 20 || IsCreative();
}

void AMCPlayer::EatFood(const FMCItem& Item)
{
	if (!Item.Food.IsValid()) return;
	const FMCFood& F = *Item.Food;
	FoodLevel = FMath::Min(20, FoodLevel + F.Nutrition);
	Saturation = FMath::Min((float)FoodLevel, Saturation + F.Nutrition * F.Saturation * 2.f);
	for (const TPair<FMCEffectInstance, float>& E : F.Effects)
	{
		if (Rand().NextFloat() < E.Value) AddEffect(E.Key);
	}
	// special foods
	if (Item.Name == TEXT("milk_bucket")) ClearEffects();
	if (Item.Name == TEXT("chorus_fruit") && World)
	{
		for (int32 Try = 0; Try < 16; ++Try)
		{
			const FVector T = Pos + FVector(Rand().FRange(-8.f, 8.f), Rand().FRange(-8.f, 8.f), (float)Rand().Range(-8, 8));
			const int32 GZ = World->FindGroundZ(MC::FloorToInt(T.X), MC::FloorToInt(T.Y), MC::FloorToInt(T.Z) + 1);
			const FVector Cand(T.X, T.Y, GZ);
			if (GZ > MC::MinZ && World->IsRegionFree(GetBoxAt(Cand)))
			{
				World->PlaySound(TEXT("chorus_fruit_teleport"), Pos, 1.f, 1.f);
				TeleportTo(Cand);
				World->PlaySound(TEXT("chorus_fruit_teleport"), Pos, 1.f, 1.f);
				break;
			}
		}
		ItemCooldown = 20;
	}
	PlaySound(TEXT("player_burp"), 0.5f, 0.9f + Rand().NextFloat() * 0.1f);
}

void AMCPlayer::TickFood()
{
	if (!Game) return;
	const EMCDifficulty Diff = Game->Difficulty;
	if (Exhaustion > 4.f)
	{
		Exhaustion -= 4.f;
		if (Saturation > 0.f) Saturation = FMath::Max(0.f, Saturation - 1.f);
		else if (Diff != EMCDifficulty::Peaceful) FoodLevel = FMath::Max(0, FoodLevel - 1);
	}
	const bool bRegen = Game->Rules.bNaturalRegeneration;
	if (Diff == EMCDifficulty::Peaceful)
	{
		if (Age % 20 == 0 && Health < GetMaxHealth()) Heal(1.f);
		if (Age % 10 == 0 && FoodLevel < 20) FoodLevel++;
	}
	if (bRegen && Saturation > 0.f && Health < GetMaxHealth() && FoodLevel >= 20)
	{
		if (++FoodTimer >= 10)
		{
			const float S = FMath::Min(Saturation, 6.f);
			Heal(S / 6.f);
			CauseExhaustion(S);
			FoodTimer = 0;
		}
	}
	else if (bRegen && FoodLevel >= 18 && Health < GetMaxHealth())
	{
		if (++FoodTimer >= 80)
		{
			Heal(1.f);
			CauseExhaustion(6.f);
			FoodTimer = 0;
		}
	}
	else if (FoodLevel <= 0)
	{
		if (++FoodTimer >= 80)
		{
			const float Min = Diff == EMCDifficulty::Hard ? 0.f : (Diff == EMCDifficulty::Normal ? 1.f : 10.f);
			if (Health > Min) Hurt(FMCDamage::Of(TEXT("starve")), 1.f);
			FoodTimer = 0;
		}
	}
	else FoodTimer = 0;
}

int32 AMCPlayer::XPNeededForNextLevel() const
{
	if (XPLevel >= 30) return 112 + (XPLevel - 30) * 9;
	if (XPLevel >= 15) return 37 + (XPLevel - 15) * 5;
	return 7 + XPLevel * 2;
}

void AMCPlayer::GiveXP(int32 Amount)
{
	if (Amount <= 0) return;
	Score += Amount;
	XPTotal += Amount;
	XPProgress += (float)Amount / XPNeededForNextLevel();
	const int32 OldLevel = XPLevel;
	while (XPProgress >= 1.f)
	{
		XPProgress = (XPProgress - 1.f) * XPNeededForNextLevel();
		++XPLevel;
		XPProgress /= XPNeededForNextLevel();
	}
	if (XPLevel > OldLevel && XPLevel % 5 == 0) PlaySound(TEXT("player_levelup"), 0.75f, 1.f);
}

void AMCPlayer::GiveXPLevels(int32 Levels)
{
	XPLevel = FMath::Max(0, XPLevel + Levels);
	if (XPLevel == 0 && Levels < 0) { XPProgress = 0.f; XPTotal = 0; }
	EnchantSeed = Rand().NextInt(INT_MAX);
}

// ---------------------------------------------------------------------------------------------------------------------
// Inventory

bool AMCPlayer::AddItem(FMCItemStack& Stack)
{
	if (Stack.IsEmpty()) return true;
	// Minecraft order: merge into matching stacks (selected, offhand, hotbar, main), then first empty slot
	auto Merge = [&](int32 I)
	{
		FMCItemStack& S = Inventory.Slots[I];
		if (S.IsEmpty() || !S.CanStackWith(Stack)) return;
		const int32 Space = S.MaxStack() - S.Count;
		const int32 N = FMath::Min(Space, Stack.Count);
		if (N <= 0) return;
		S.Count += N;
		Stack.Count -= N;
	};
	Merge(Selected);
	Merge(MCInv::Offhand);
	for (int32 i = 0; i < 36 && Stack.Count > 0; ++i) Merge(i);
	for (int32 i = 0; i < 36 && Stack.Count > 0; ++i)
	{
		FMCItemStack& S = Inventory.Slots[i];
		if (!S.IsEmpty()) continue;
		const int32 N = FMath::Min(Stack.MaxStack(), Stack.Count);
		S = Stack.Copy();
		S.Count = N;
		Stack.Count -= N;
	}
	if (Stack.Count <= 0) { Stack.Clear(); return true; }
	return false;
}

void AMCPlayer::GiveItem(const FMCItemStack& In)
{
	FMCItemStack S = In.Copy();
	if (!AddItem(S)) DropStack(S, false);
	PlaySound(TEXT("item_pickup"), 0.2f, 1.4f + Rand().FRange(-0.2f, 0.4f));
}

void AMCPlayer::DropStack(const FMCItemStack& Stack, bool bThrow)
{
	if (Stack.IsEmpty() || !World || !Game) return;
	AMCItemEntity* IE = Game->SpawnEntity<AMCItemEntity>(World, GetEyePos() - FVector(0, 0, 0.3));
	if (!IE) return;
	IE->SetStack(Stack);
	IE->PickupDelay = 40;
	IE->Thrower = this;
	if (bThrow)
	{
		const FVector Look = GetLookDir();
		IE->Vel = Look * 0.3 + FVector(Rand().FRange(-0.02f, 0.02f), Rand().FRange(-0.02f, 0.02f), 0.1);
	}
	else
	{
		const float A = Rand().NextFloat() * 2.f * PI;
		const float F = Rand().NextFloat() * 0.5f;
		IE->Vel = FVector(FMath::Cos(A) * F, FMath::Sin(A) * F, 0.2);
	}
}

void AMCPlayer::DropSelected(bool bWholeStack)
{
	FMCItemStack& H = Held();
	if (H.IsEmpty()) return;
	FMCItemStack D = bWholeStack ? H.Split(H.Count) : H.Split(1);
	DropStack(D, true);
	Swing();
}

void AMCPlayer::SwapHands()
{
	Swap(Inventory.Slots[Selected], Inventory.Slots[MCInv::Offhand]);
	StopUsingItem(false);
}

void AMCPlayer::SelectSlot(int32 Slot)
{
	Slot = ((Slot % 9) + 9) % 9;
	if (Slot == Selected) return;
	Selected = Slot;
	StopMining();
	StopUsingItem(false);
	EquipAnim = 0.f;
	++NumSelectedChanges;
}

void AMCPlayer::ScrollHotbar(int32 Delta)
{
	if (Delta == 0) return;
	SelectSlot(Selected + (Delta > 0 ? 1 : -1));
}

int32 AMCPlayer::FindSlot(FMCItemId Id) const
{
	for (int32 i = 0; i < 36; ++i) if (!Inventory.Slots[i].IsEmpty() && Inventory.Slots[i].Id == Id) return i;
	return -1;
}

void AMCPlayer::ConsumeHeld(int32 Amount, bool bOffhand)
{
	if (IsCreative()) return;
	FMCItemStack& S = bOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	S.Count -= Amount;
	if (S.Count <= 0) S.Clear();
}

void AMCPlayer::ReplaceHeld(const FMCItemStack& NewStack, bool bOffhand)
{
	FMCItemStack& S = bOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	if (IsCreative())
	{
		// creative keeps the original item and adds the result if missing
		if (FindSlot(NewStack.Id) < 0) { FMCItemStack Copy = NewStack.Copy(); AddItem(Copy); }
		return;
	}
	if (S.Count <= 1) { S = NewStack.Copy(); return; }
	S.Count -= 1;
	GiveItem(NewStack);
}

void AMCPlayer::DamageHeld(int32 Amount, bool bOffhand)
{
	if (IsCreative()) return;
	FMCItemStack& S = bOffhand ? Inventory.Slots[MCInv::Offhand] : Held();
	if (S.IsEmpty() || !S.IsDamageable()) return;
	if (S.DamageItem(Amount, Rand())) PlaySound(TEXT("item_break"), 0.8f, 0.8f + Rand().NextFloat() * 0.4f);
}

void AMCPlayer::PickBlock()
{
	FMCRayHit Hit;
	AMCEntity* E = nullptr;
	if (!GetTarget(Hit, E)) return;
	FMCItemStack Want;
	if (E)
	{
		if (AMCMob* M = Cast<AMCMob>(E))
		{
			if (M->Def) Want = FMCItemStack::Of(FName(*(M->Def->Id.ToString() + TEXT("_spawn_egg"))), 1);
		}
		else if (AMCBoat* B = Cast<AMCBoat>(E)) Want = FMCItemStack::Of(B->ItemName(), 1);
		else if (AMCMinecart* C = Cast<AMCMinecart>(E)) Want = FMCItemStack::Of(C->Variant, 1);
		else if (E->IsA<AMCEndCrystal>()) Want = FMCItemStack::Of(TEXT("end_crystal"), 1);
		else if (E->IsA<AMCItemFrame>()) Want = FMCItemStack::Of(TEXT("item_frame"), 1);
	}
	else if (Hit.bHit)
	{
		const FMCBlock& B = FMCBlocks::GetByState(Hit.State);
		if (!B.ItemName.IsNone()) Want = FMCItemStack::Of(B.ItemName, 1);
		else Want = FMCItemStack(FMCItems::ForBlock(B.Id), 1);
	}
	if (Want.IsEmpty()) return;
	const int32 Found = FindSlot(Want.Id);
	if (Found >= 0 && Found < 9) { SelectSlot(Found); return; }
	if (IsCreative())
	{
		Want.Count = 1;
		int32 Target = Selected;
		if (!Held().IsEmpty())
		{
			for (int32 i = 0; i < 9; ++i) if (Inventory.Slots[i].IsEmpty()) { Target = i; break; }
		}
		Inventory.Slots[Target] = Want;
		SelectSlot(Target);
		return;
	}
	if (Found >= 9) Swap(Inventory.Slots[Found], Inventory.Slots[Selected]);
}

// ---------------------------------------------------------------------------------------------------------------------
// Menus

void AMCPlayer::OpenMenu(TSharedPtr<FMCMenu> NewMenu)
{
	if (Menu.IsValid() && Menu != NewMenu) Menu->Removed();
	bInventoryOpen = false;
	Menu = NewMenu;
	StopMining();
	StopUsingItem(false);
}

void AMCPlayer::OpenInventory()
{
	if (Menu.IsValid()) CloseMenu();
	if (!InvMenu.IsValid()) InvMenu = MakeShared<FMCInventoryMenu>(this);
	bInventoryOpen = true;
	StopMining();
}

void AMCPlayer::CloseMenu()
{
	if (Menu.IsValid())
	{
		TSharedPtr<FMCMenu> Old = Menu;
		Menu.Reset();
		Old->Removed();
	}
	if (bInventoryOpen && InvMenu.IsValid())
	{
		bInventoryOpen = false;
		InvMenu->Removed();
	}
	bInventoryOpen = false;
	if (!CarriedStack.IsEmpty())
	{
		FMCItemStack C = CarriedStack;
		CarriedStack.Clear();
		if (!AddItem(C)) DropStack(C, true);
	}
}

bool AMCPlayer::OpenBlockContainer(const FMCBlockPos& P)
{
	if (!World) return false;
	TSharedPtr<FMCBlockEntity> BE = World->GetBlockEntityShared(P);
	const FMCState S = World->GetState(P);
	const FMCBlock& B = FMCBlocks::GetByState(S);
	if (!BE.IsValid())
	{
		BE = B.Behavior->CreateBlockEntity(P, S);
		if (!BE.IsValid()) return false;
		World->SetBlockEntity(P, BE);
	}
	BE->UnpackLoot(*World);
	const FString Title = !BE->CustomName.IsEmpty() ? BE->CustomName : B.DisplayName;
	TSharedPtr<FMCMenu> M;
	switch (BE->Type)
	{
	case EMCBlockEntityType::Chest:
	case EMCBlockEntityType::TrappedChest:
	case EMCBlockEntityType::CopperChest:
	{
		// double chest: the "left" half comes first
		const int32 T = (FMCBlocks::MetaOf(S) >> 2) & 3;
		const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
		if (T != 0)
		{
			const FMCBlockPos Other = T == 1 ? P.Offset(MC::RotateY(F, 1)) : P.Offset(MC::RotateY(F, 3));
			TSharedPtr<FMCBlockEntity> BE2 = World->GetBlockEntityShared(Other);
			if (!BE2.IsValid() && FMCBlocks::BlockOf(World->GetState(Other)) == B.Id)
			{
				BE2 = B.Behavior->CreateBlockEntity(Other, World->GetState(Other));
				if (BE2.IsValid()) World->SetBlockEntity(Other, BE2);
			}
			if (BE2.IsValid() && BE2->GetContainer())
			{
				BE2->UnpackLoot(*World);
				FMCContainer* A = T == 1 ? BE->GetContainer() : BE2->GetContainer();
				FMCContainer* Bc = T == 1 ? BE2->GetContainer() : BE->GetContainer();
				TSharedPtr<FMCChestMenu> CM = MakeShared<FMCChestMenu>(this, EMCMenuType::Chest, A, Bc, 6, BE->CustomName.IsEmpty() ? FString(TEXT("Large Chest")) : BE->CustomName);
				CM->Viewed = { BE, BE2 };
				CM->Pos = P; CM->bHasPos = true;
				M = CM;
				break;
			}
		}
		TSharedPtr<FMCChestMenu> CM = MakeShared<FMCChestMenu>(this, EMCMenuType::Chest, BE->GetContainer(), nullptr, 3, Title);
		CM->Viewed = { BE };
		CM->Pos = P; CM->bHasPos = true;
		M = CM;
		break;
	}
	case EMCBlockEntityType::Barrel:
	{
		TSharedPtr<FMCChestMenu> CM = MakeShared<FMCChestMenu>(this, EMCMenuType::Chest, BE->GetContainer(), nullptr, 3, Title);
		CM->Viewed = { BE }; CM->Pos = P; CM->bHasPos = true;
		M = CM;
		break;
	}
	case EMCBlockEntityType::ShulkerBox:
	{
		TSharedPtr<FMCChestMenu> CM = MakeShared<FMCChestMenu>(this, EMCMenuType::Shulker, BE->GetContainer(), nullptr, 3, Title);
		CM->Viewed = { BE }; CM->Pos = P; CM->bHasPos = true;
		M = CM;
		break;
	}
	case EMCBlockEntityType::Furnace:
	case EMCBlockEntityType::BlastFurnace:
	case EMCBlockEntityType::Smoker:
		M = MakeShared<FMCFurnaceMenu>(this, StaticCastSharedPtr<FMCFurnaceEntity>(BE));
		break;
	case EMCBlockEntityType::BrewingStand:
		M = MakeShared<FMCBrewingMenu>(this, StaticCastSharedPtr<FMCBrewingEntity>(BE));
		break;
	case EMCBlockEntityType::Hopper:
		M = MakeShared<FMCSimpleContainerMenu>(this, EMCMenuType::Hopper, BE, Title);
		break;
	case EMCBlockEntityType::Dispenser:
	case EMCBlockEntityType::Dropper:
		M = MakeShared<FMCSimpleContainerMenu>(this, EMCMenuType::Dispenser, BE, Title);
		break;
	case EMCBlockEntityType::Crafter:
		M = MakeShared<FMCSimpleContainerMenu>(this, EMCMenuType::Crafter, BE, Title);
		break;
	case EMCBlockEntityType::Beacon:
		M = MakeShared<FMCBeaconMenu>(this, StaticCastSharedPtr<FMCBeaconEntity>(BE));
		break;
	default:
		if (BE->GetContainer())
		{
			TSharedPtr<FMCChestMenu> CM = MakeShared<FMCChestMenu>(this, EMCMenuType::Chest, BE->GetContainer(), nullptr, FMath::Max(1, BE->GetContainer()->Num() / 9), Title);
			CM->Viewed = { BE }; CM->Pos = P; CM->bHasPos = true;
			M = CM;
		}
		break;
	}
	if (!M.IsValid()) return false;
	M->Pos = P; M->bHasPos = true;
	if (!M->BE.IsValid()) M->BE = BE;
	OpenMenu(M);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Sleeping / spawn / messages

void AMCPlayer::Sleep(const FMCBlockPos& Bed)
{
	if (!Game || !World) return;
	if (World->Dim != EMCDimension::Overworld) return;
	if (Game->IsDay() && !Game->bThundering)
	{
		ShowActionBar(TEXT("You can sleep only at night or during thunderstorms"));
		SetSpawnPoint(Bed, World->Dim, false);
		return;
	}
	if (FVector::Dist(Pos, FVector(Bed.X + 0.5, Bed.Y + 0.5, Bed.Z)) > 3.0) { ShowActionBar(TEXT("You may not rest now; the bed is too far away")); return; }
	if (IsSurvivalLike())
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(FMCBox(FVector(Bed.X - 8, Bed.Y - 8, Bed.Z - 5), FVector(Bed.X + 9, Bed.Y + 9, Bed.Z + 6)), Near, this);
		for (AMCEntity* E : Near)
		{
			const AMCMob* M = Cast<AMCMob>(E);
			if (M && M->IsAlive() && M->Def && M->Def->Category == EMCMobCategory::Monster && M->Def->bHostile)
			{
				ShowActionBar(TEXT("You may not rest now; there are monsters nearby"));
				return;
			}
		}
	}
	SetSpawnPoint(Bed, World->Dim, false);
	StopRiding();
	bSleeping = true;
	BedPos = Bed;
	SleepCounter = 0;
	TeleportTo(FVector(Bed.X + 0.5, Bed.Y + 0.5, Bed.Z + 0.5625));
}

void AMCPlayer::WakeUp(bool bSetSpawn)
{
	if (!bSleeping) return;
	bSleeping = false;
	SleepCounter = 0;
	if (World)
	{
		// stand next to the bed
		for (int32 dx = -1; dx <= 1; ++dx)
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				const FVector C(BedPos.X + dx + 0.5, BedPos.Y + dy + 0.5, BedPos.Z + 0.6);
				if (World->IsRegionFree(GetBoxAt(C))) { TeleportTo(C); return; }
			}
		TeleportTo(FVector(BedPos.X + 0.5, BedPos.Y + 0.5, BedPos.Z + 1.0));
	}
}

void AMCPlayer::SetSpawnPoint(const FMCBlockPos& P, EMCDimension Dim, bool bForced)
{
	const bool bChanged = !bHasSpawnPoint || !(SpawnPoint == P) || SpawnDim != Dim;
	SpawnPoint = P;
	SpawnDim = Dim;
	bHasSpawnPoint = true;
	bSpawnForced = bForced;
	if (bChanged) SendMessage(TEXT("Respawn point set"));
}

void AMCPlayer::SendMessage(const FString& Msg) const
{
	if (Game) Game->AddChat(Msg);
}

void AMCPlayer::ShowActionBar(const FString& Msg) const
{
	if (Game) Game->ShowActionBar(Msg);
}

void AMCPlayer::HandlePortalTransitions()
{
	if (bInsidePortal && PortalCooldown == 0)
	{
		PortalOverlay = FMath::Min(1.f, PortalOverlay + 0.0125f);
		if (PortalTime == 1 || (PortalTime > 0 && PortalTime % 80 == 1)) PlaySound(TEXT("portal_trigger"), 0.25f, 0.8f + Rand().NextFloat() * 0.4f);
	}
	else
	{
		PortalOverlay = FMath::Max(0.f, PortalOverlay - 0.05f);
	}
}

void AMCPlayer::OnDimensionChanged()
{
	StopMining();
	PortalTime = 0;
	PortalOverlay = 0.f;
	if (Menu.IsValid()) CloseMenu();
	bElytraFlying = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Persistence

void AMCPlayer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	int32 Version = 1;
	Ar << Version;
	Inventory.Serialize(Ar);
	EnderInventory.Serialize(Ar);
	Ar << Selected;
	uint8 Mode = (uint8)GameMode; Ar << Mode; if (Ar.IsLoading()) GameMode = (EMCGameMode)Mode;
	Ar << bFlying << FoodLevel << Saturation << Exhaustion << XPLevel << XPProgress << XPTotal << Score << EnchantSeed;
	Ar << SpawnPoint << bHasSpawnPoint << bSpawnForced;
	uint8 SD = (uint8)SpawnDim; Ar << SD; if (Ar.IsLoading()) SpawnDim = (EMCDimension)SD;
	Ar << StatBlocksMined << StatBlocksPlaced << StatMobsKilled << StatDeaths << StatDistanceWalked;
	Ar << KnownRecipes << bSeenCredits << ViewYaw << ViewPitch;
	if (Ar.IsLoading())
	{
		Selected = FMath::Clamp(Selected, 0, 8);
		if (Inventory.Num() != MCInv::Size) Inventory.Init(MCInv::Size);
		if (EnderInventory.Num() != 27) EnderInventory.Init(27);
		Yaw = ViewYaw; Pitch = ViewPitch;
	}
}
