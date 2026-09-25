// Mob core: definition setup, equipment, visuals, ticking, damage/death/loot, breeding, taming, interaction, persistence.
#include "Game/MCMob.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCEntities.h"
#include "Render/MCRig.h"
#include "Items/MCLoot.h"
#include "World/MCWorld.h"

namespace
{
	const FColor SheepColors[16] = {
		FColor(233, 236, 236), FColor(240, 118, 19), FColor(189, 68, 179), FColor(58, 175, 217), FColor(248, 198, 39), FColor(112, 185, 25),
		FColor(237, 141, 172), FColor(62, 68, 71), FColor(142, 142, 134), FColor(21, 137, 145), FColor(121, 42, 172), FColor(53, 57, 157),
		FColor(114, 71, 40), FColor(84, 109, 27), FColor(161, 39, 34), FColor(20, 21, 25) };

	FName FallbackRig(const FMCMobDef& D)
	{
		switch (D.AI)
		{
		case EMCMobAI::Zombie: case EMCMobAI::Skeleton: case EMCMobAI::Villager: case EMCMobAI::Illager: return TEXT("zombie");
		case EMCMobAI::Animal: case EMCMobAI::Mount: case EMCMobAI::Tameable: return TEXT("pig");
		case EMCMobAI::Flyer: return TEXT("bat");
		case EMCMobAI::Swimmer: return TEXT("cod");
		case EMCMobAI::Slime: return TEXT("slime");
		case EMCMobAI::Spider: case EMCMobAI::Silverfish: return TEXT("spider");
		default: return TEXT("zombie");
		}
	}

	FName SoundFor(const FMCMobDef* D, const TCHAR* Suffix)
	{
		if (!D) return NAME_None;
		return FName(*FString::Printf(TEXT("%s_%s"), *D->Sound.ToString(), Suffix));
	}
}

AMCMob::AMCMob()
{
	Kind = EMCEntityKind::Mob;
	Rig = CreateDefaultSubobject<UMCRigComponent>(TEXT("Rig"));
	Rig->SetupAttachment(VisualRoot);
}

void AMCMob::SetDefinition(const FMCMobDef* InDef, int32 InVariant)
{
	Def = InDef;
	if (!Def) return;
	TypeId = Def->Id;
	Width = Def->Width;
	Height = Def->Height;
	EyeHeight = Def->EyeHeight;
	MaxHealth = Def->MaxHealth;
	MoveSpeed = Def->Speed;
	BaseArmor = Def->Armor;
	KnockbackResistance = Def->KnockbackResist;
	bUndead = Def->bUndead;
	bArthropod = Def->bArthropod;
	bFireImmune = Def->bFireImmune;
	bNoGravity = Def->bNoGravity;
	bCanBreatheUnderwater = Def->bBreathesUnderwater || Def->bUndead;
	StepHeight = Def->StepHeight;
	FMCRandom& R = Rand();
	Variant = InVariant >= 0 ? InVariant : (Def->NumVariants > 1 ? R.NextInt(Def->NumVariants) : 0);
	const FName Id = Def->Id;
	if (Def->AI == EMCMobAI::Slime)
	{
		// sizes 1, 2 or 4 (sulfur cubes are always medium)
		SplitSize = Id == TEXT("sulfur_cube") ? 2 : (1 << R.NextInt(3));
		Width = Height = 0.52f * SplitSize;
		EyeHeight = Height * 0.625f;
		MaxHealth = (float)(SplitSize * SplitSize);
		MoveSpeed = 0.2f + 0.1f * SplitSize;
		BaseArmor = Id == TEXT("magma_cube") ? 3.f * SplitSize : 0.f;
	}
	if (Id == TEXT("sheep"))
	{
		// natural wool colour distribution
		const int32 Roll = R.NextInt(100);
		DyeColor = Roll < 5 ? 15 : (Roll < 10 ? 7 : (Roll < 15 ? 8 : (Roll < 18 ? 12 : (R.NextInt(500) == 0 ? 6 : 0))));
		Color = SheepColors[DyeColor];
	}
	if (Def->AI == EMCMobAI::Mount && (Id == TEXT("horse") || Id == TEXT("donkey") || Id == TEXT("mule")))
	{
		MaxHealth = 15.f + R.NextInt(8) + R.NextInt(9);
		MoveSpeed = (0.45f + R.NextFloat() * 0.3f + R.NextFloat() * 0.3f + R.NextFloat() * 0.3f) * 0.25f;
		JumpPower = 0.4f + R.NextFloat() * 0.2f + R.NextFloat() * 0.2f + R.NextFloat() * 0.2f;
	}
	if (Id == TEXT("villager")) GenerateTrades();
	if (Id == TEXT("wandering_trader")) GenerateTrades();
	Health = MaxHealth;
	if (Id == TEXT("bat") || Id == TEXT("ghast") || Id == TEXT("phantom") || Id == TEXT("vex") || Id == TEXT("allay") || Id == TEXT("parrot") || Id == TEXT("bee") || Id == TEXT("happy_ghast") || Id == TEXT("ghastling") || Id == TEXT("blaze") || Id == TEXT("breeze"))
	{
		Gravity = (Id == TEXT("blaze") || Id == TEXT("breeze") || Id == TEXT("parrot")) ? 0.04 : 0.0;
		bNoGravity = !(Id == TEXT("blaze") || Id == TEXT("breeze") || Id == TEXT("parrot"));
	}
}

float AMCMob::GetSpeed() const
{
	float S = Super::GetSpeed() * SpeedModifier;
	if (IsBaby() && Def && Def->Category == EMCMobCategory::Monster) S *= 1.5f; // baby zombies are fast
	return S;
}

bool AMCMob::IsAffectedByPotions() const
{
	return !(Def && (Def->bBoss || Def->Id == TEXT("warden")));
}

bool AMCMob::CanBreatheUnderwater() const
{
	return Super::CanBreatheUnderwater() || (Def && (Def->bSwimmer || Def->bAmphibious || Def->bUndead || Def->AI == EMCMobAI::Golem));
}

void AMCMob::InitEntity()
{
	Super::InitEntity();
	if (!Def) return;
	FName RigId = Def->Rig;
	if (!MCRigs::Find(RigId)) RigId = FallbackRig(*Def);
	Rig->SetRig(RigId, Def->Tint);
	Rig->ModelScale = (Rig->Rig ? Rig->Rig->Scale : 1.f) * Def->Scale;
	// natural equipment (Minecraft chances simplified)
	FMCRandom& R = Rand();
	const FName Id = Def->Id;
	auto Give = [&](EMCEquipSlot S, const TCHAR* Item) { if (GetItem(S).IsEmpty()) GetItem(S) = FMCItemStack::Of(FName(Item), 1); };
	if (Id == TEXT("skeleton") || Id == TEXT("stray") || Id == TEXT("bogged") || Id == TEXT("parched")) Give(EMCEquipSlot::MainHand, TEXT("bow"));
	else if (Id == TEXT("pillager")) Give(EMCEquipSlot::MainHand, TEXT("crossbow"));
	else if (Id == TEXT("vindicator")) Give(EMCEquipSlot::MainHand, TEXT("iron_axe"));
	else if (Id == TEXT("wither_skeleton")) Give(EMCEquipSlot::MainHand, TEXT("stone_sword"));
	else if (Id == TEXT("zombified_piglin")) Give(EMCEquipSlot::MainHand, TEXT("golden_sword"));
	else if (Id == TEXT("piglin_brute")) Give(EMCEquipSlot::MainHand, TEXT("golden_axe"));
	else if (Id == TEXT("piglin")) Give(EMCEquipSlot::MainHand, R.NextBool() ? TEXT("golden_sword") : TEXT("crossbow"));
	else if (Id == TEXT("drowned") && R.NextInt(100) < 6) Give(EMCEquipSlot::MainHand, TEXT("trident"));
	else if (Id == TEXT("drowned") && R.NextInt(100) < 3) Give(EMCEquipSlot::OffHand, TEXT("nautilus_shell"));
	else if ((Id == TEXT("zombie") || Id == TEXT("husk")) && R.NextInt(100) < 2) Give(EMCEquipSlot::MainHand, R.NextBool() ? TEXT("iron_sword") : TEXT("iron_shovel"));
	else if (Id == TEXT("vex")) Give(EMCEquipSlot::MainHand, TEXT("iron_sword"));
	if (Def->Category == EMCMobCategory::Monster && (Def->AI == EMCMobAI::Zombie || Def->AI == EMCMobAI::Skeleton) && R.NextInt(100) < 8 && Id != TEXT("piglin") && Id != TEXT("hoglin") && Id != TEXT("zoglin") && Id != TEXT("creaking"))
	{
		static const TCHAR* Mats[] = { TEXT("leather"), TEXT("golden"), TEXT("chainmail"), TEXT("iron") };
		const TCHAR* M = Mats[R.NextInt(4)];
		static const TCHAR* Pieces[] = { TEXT("_helmet"), TEXT("_chestplate"), TEXT("_leggings"), TEXT("_boots") };
		static const EMCEquipSlot Slots[] = { EMCEquipSlot::Head, EMCEquipSlot::Chest, EMCEquipSlot::Legs, EMCEquipSlot::Feet };
		for (int32 p = 0; p < 4; ++p) if (p == 0 || R.NextInt(100) < 40) Give(Slots[p], *(FString(M) + Pieces[p]));
	}
	if (Id == TEXT("fox") && R.NextInt(100) < 20) Give(EMCEquipSlot::MainHand, R.NextBool() ? TEXT("sweet_berries") : TEXT("emerald"));
	if (Id == TEXT("donkey") || Id == TEXT("mule") || Id == TEXT("llama") || Id == TEXT("trader_llama")) MobInventory.Init(15);
	if (Id == TEXT("allay") || Id == TEXT("piglin")) MobInventory.Init(8);
	if (Id == TEXT("villager") || Id == TEXT("wandering_trader")) MobInventory.Init(8);
	// held item visual
	const FMCItemStack& Held = GetItem(EMCEquipSlot::MainHand);
	if (!Held.IsEmpty())
	{
		HeldVisual = NewObject<UMCItemVisualComponent>(this, TEXT("HeldVisual"));
		HeldVisual->SetupAttachment(Rig);
		HeldVisual->RegisterComponent();
		HeldVisual->SetStack(Held, 2);
		const FName Arm = Rig->Rig && Rig->Rig->RightArm >= 0 ? Rig->Rig->Parts[Rig->Rig->RightArm].Name : FName(TEXT("head"));
		const FVector3f HO = Rig->Rig ? Rig->Rig->HandOffset : FVector3f(0, 0, -0.62f);
		Rig->AttachToPart(HeldVisual, Arm, FVector(HO) + FVector(0.06, 0, 0), FRotator(0, 0, -90));
		HeldVisual->SetRelativeScale3D(FVector(0.55));
	}
	ApplyVariantVisuals();
	FlyTarget = Pos;
	AmbientSoundTime = -Rand().NextInt(200);
}

namespace
{
	/** Coat colours per species for the variant system (0 = default), Minecraft-like natural ranges. */
	const TArray<FColor>* VariantCoats(FName Id)
	{
		static TMap<FName, TArray<FColor>> Coats;
		if (Coats.Num() == 0)
		{
			// temperate / warm / cold farm animals
			Coats.Add(TEXT("pig"), { FColor(240, 170, 160), FColor(212, 128, 86), FColor(176, 166, 150) });
			Coats.Add(TEXT("cow"), { FColor(110, 76, 52), FColor(176, 92, 58), FColor(146, 134, 122) });
			Coats.Add(TEXT("chicken"), { FColor(240, 240, 236), FColor(214, 146, 86), FColor(196, 196, 188) });
			// pale, woods, ashen, black, chestnut, rusty, spotted, striped, snowy
			Coats.Add(TEXT("wolf"), { FColor(226, 222, 212), FColor(122, 96, 70), FColor(150, 150, 150), FColor(48, 46, 50), FColor(142, 86, 52),
				FColor(178, 92, 52), FColor(202, 172, 132), FColor(162, 132, 96), FColor(242, 242, 242) });
			Coats.Add(TEXT("cat"), { FColor(172, 132, 92), FColor(44, 42, 46), FColor(222, 132, 62), FColor(232, 216, 190), FColor(142, 146, 152),
				FColor(232, 202, 162), FColor(242, 212, 162), FColor(236, 230, 220), FColor(246, 246, 246), FColor(122, 122, 122), FColor(26, 26, 30) });
			Coats.Add(TEXT("horse"), { FColor(236, 236, 230), FColor(222, 192, 142), FColor(162, 92, 52), FColor(112, 72, 46), FColor(42, 40, 40),
				FColor(132, 132, 132), FColor(72, 46, 30) });
			Coats.Add(TEXT("rabbit"), { FColor(132, 96, 66), FColor(242, 242, 242), FColor(42, 42, 42), FColor(202, 202, 202), FColor(222, 182, 102), FColor(162, 152, 142) });
			Coats.Add(TEXT("llama"), { FColor(232, 212, 172), FColor(242, 242, 236), FColor(122, 86, 56), FColor(142, 142, 142) });
			Coats.Add(TEXT("trader_llama"), { FColor(232, 212, 172), FColor(242, 242, 236), FColor(122, 86, 56), FColor(142, 142, 142) });
			Coats.Add(TEXT("axolotl"), { FColor(242, 162, 192), FColor(132, 102, 82), FColor(242, 202, 82), FColor(172, 232, 242), FColor(82, 112, 222) });
			Coats.Add(TEXT("parrot"), { FColor(222, 42, 42), FColor(42, 82, 222), FColor(82, 202, 62), FColor(62, 202, 212), FColor(172, 172, 172) });
			Coats.Add(TEXT("frog"), { FColor(202, 122, 62), FColor(232, 232, 222), FColor(92, 152, 82) });
			Coats.Add(TEXT("fox"), { FColor(222, 122, 52), FColor(242, 242, 242) });
			Coats.Add(TEXT("mooshroom"), { FColor(182, 42, 42), FColor(152, 112, 72) });
			// copper golem: fresh, exposed, weathered, oxidized
			Coats.Add(TEXT("copper_golem"), { FColor(202, 112, 72), FColor(172, 132, 102), FColor(102, 152, 122), FColor(82, 172, 142) });
		}
		return Coats.Find(Id);
	}
}

void AMCMob::ApplyVariantVisuals()
{
	if (!Def || !Rig) return;
	const FName Id = Def->Id;
	FColor C = Def->Tint;
	if (Id == TEXT("sheep")) C = bSheared ? FColor(215, 190, 165) : Color;
	else if (const TArray<FColor>* Coats = VariantCoats(Id))
	{
		// natural per-species coats (variant 0 = the species' default look)
		if (Variant > 0 && Coats->Num() > 0) C = (*Coats)[Variant % Coats->Num()];
	}
	else if (Id == TEXT("tropical_fish") && Variant > 0)
	{
		static const FColor Reef[] = { FColor(250, 120, 40), FColor(60, 140, 230), FColor(250, 220, 60), FColor(230, 80, 140), FColor(90, 200, 120), FColor(240, 240, 240) };
		const FColor P = Reef[Variant % UE_ARRAY_COUNT(Reef)];
		C = FColor((C.R + P.R) / 2, (C.G + P.G) / 2, (C.B + P.B) / 2);
	}
	else if (Id == TEXT("creeper") && bCharged) C = FColor(90, 200, 255);
	Rig->SetTint(C);
	Rig->SetGlow(Id == TEXT("glow_squid") || Id == TEXT("blaze") || Id == TEXT("magma_cube") || Id == TEXT("allay") || Id == TEXT("vex") || (Id == TEXT("creeper") && bCharged) ? 0.6f : 0.f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick

void AMCMob::TickEntity()
{
	if (!Def) { Super::TickEntity(); return; }
	PrevSwellTime = SwellTime;
	if (AttackCooldown > 0) --AttackCooldown;
	if (TeleportCooldown > 0) --TeleportCooldown;
	if (PanicTime > 0) --PanicTime;
	if (AngerTime > 0 && --AngerTime == 0) { bAngry = false; if (!Def->bHostile) Target.Reset(); }
	if (InLove > 0)
	{
		--InLove;
		if (InLove % 10 == 0 && World) World->SpawnParticles(TEXT("heart"), Pos + FVector(Rand().FRange(-Width, Width) * 0.5, Rand().FRange(-Width, Width) * 0.5, Height + 0.3), 1, 0.f);
	}
	if (GrowAge < 0) { if (++GrowAge == 0) { /* grown up */ } }
	else if (GrowAge > 0) --GrowAge;

	Super::TickEntity();
	if (bRemoved || !World) return;

	// daylight burning
	if (Def->bBurnsInDaylight && IsAlive() && IsSunBurnTick())
	{
		FMCItemStack& Helm = GetItem(EMCEquipSlot::Head);
		if (!Helm.IsEmpty() && Helm.IsDamageable()) { if (Helm.DamageItem(Rand().NextInt(2), Rand())) BreakItem(EMCEquipSlot::Head); }
		else SetOnFire(8);
	}
	// water hurts endermen, blazes, snow golems, striders get cold
	if (IsSensitiveToWater() && (bInWater || (Game && Game->IsRainingAt(BlockPos()) && World->CanSeeSky(BlockPos()))) && Age % 10 == 0)
	{
		Hurt(FMCDamage::Of(TEXT("drown")), 1.f);
		if (Def->Id == TEXT("enderman")) TeleportRandomly(32.f);
	}
	// fish suffocate on land
	if (Def->bSwimmer && !Def->bAmphibious && !bInWater && IsAlive())
	{
		if (--AirSupply < -20) { AirSupply = 0; Hurt(FMCDamage::Of(TEXT("drown")), 2.f); }
	}
	else if (Def->bSwimmer) AirSupply = MaxAir;
	UpdateAmbientSound();

	// despawning
	if (Game && Game->Player && !bPersistent && CustomName.IsEmpty() && !bTamed)
	{
		const double DSq = FVector::DistSquared(Game->Player->Pos, Pos);
		if (ShouldDespawn(DSq)) { Discard(); return; }
	}
	++NoActionTime;
	if (Game && Game->Player && FVector::DistSquared(Game->Player->Pos, Pos) < 1024.0) NoActionTime = FMath::Min(NoActionTime, 600);

	// carry passengers
	for (TWeakObjectPtr<AMCEntity>& P : Passengers)
	{
		if (AMCEntity* E = P.Get())
		{
			E->PrevPos = E->Pos;
			E->Pos = Pos + GetPassengerOffset(E);
			E->FallDistance = 0.f;
		}
	}
	// leash
	if (AMCEntity* L = LeashHolder.Get())
	{
		const double D = FVector::Dist(L->Pos, Pos);
		if (D > 10.0) { LeashHolder.Reset(); if (World) World->SpawnItem(Pos, FMCItemStack::Of(TEXT("lead"), 1)); }
		else if (D > 6.0) Vel += (L->Pos - Pos).GetSafeNormal() * 0.04 * (D - 6.0);
	}
}

bool AMCMob::ShouldDespawn(double DistSq) const
{
	if (!Def) return false;
	const EMCMobCategory C = Def->Category;
	if (C == EMCMobCategory::Creature || C == EMCMobCategory::Misc || C == EMCMobCategory::Boss) return false;
	if (Def->Id == TEXT("villager") || Def->Id == TEXT("shulker") || Def->Id == TEXT("elder_guardian") || Def->Id == TEXT("warden")) return false;
	if (!bNaturalSpawn && C != EMCMobCategory::Monster && C != EMCMobCategory::WaterAmbient && C != EMCMobCategory::Ambient) return false;
	if (DistSq > 128.0 * 128.0) return true;
	if (DistSq > 32.0 * 32.0 && NoActionTime > 600 && Rand().NextInt(800) == 0) return true;
	if (Game && Game->Difficulty == EMCDifficulty::Peaceful && C == EMCMobCategory::Monster && !Def->bBoss) return true;
	return false;
}

void AMCMob::UpdateAmbientSound()
{
	if (!Def || !IsAlive() || bSilent) return;
	if (Rand().NextInt(1000) < AmbientSoundTime++)
	{
		AmbientSoundTime = -80;
		if (Def->Id == TEXT("enderman") && bAngry) PlaySound(TEXT("enderman_scream"), 1.f, 1.f);
		else PlaySound(SoundFor(Def, TEXT("ambient")), Def->Category == EMCMobCategory::Ambient ? 0.1f : 1.f, IsBaby() ? 1.4f + Rand().NextFloat() * 0.2f : 0.9f + Rand().NextFloat() * 0.2f);
	}
}

void AMCMob::AIStep()
{
	if (!Def || !IsAlive() || !World) { Super::AIStep(); return; }
	if (bNoAI)
	{
		// Minecraft NoAI: the mob keeps its pose, ignores goals and is not moved by physics
		MoveForward = MoveStrafe = MoveUp = 0.f;
		bJumping = false;
		Vel = FVector::ZeroVector;
		return;
	}
	bJumping = false;
	SpeedModifier = 1.f;
	// ridden & controlled mounts are driven by their rider
	AMCPlayer* Rider = Passengers.Num() > 0 ? Cast<AMCPlayer>(Passengers[0].Get()) : nullptr;
	const FName Id = Def->Id;
	const bool bControllable = Rider && ((Def->AI == EMCMobAI::Mount && (bSaddled || Id == TEXT("llama") || Id == TEXT("trader_llama") ? bSaddled || bTamed : false))
		|| (Id == TEXT("pig") && bSaddled && Rider->HeldConst().Item().Name == TEXT("carrot_on_a_stick"))
		|| (Id == TEXT("strider") && bSaddled && Rider->HeldConst().Item().Name == TEXT("warped_fungus_on_a_stick"))
		|| (Id == TEXT("happy_ghast") && bHarnessed) || (Id == TEXT("camel") && bSaddled) || (Id == TEXT("camel_husk")) || (Id == TEXT("nautilus") && bSaddled));
	if (bControllable)
	{
		Yaw = Rider->ViewYaw;
		Pitch = Rider->ViewPitch * 0.5f;
		if (Id == TEXT("pig") || Id == TEXT("strider"))
		{
			MoveForward = 1.f;
			SpeedModifier = Id == TEXT("pig") ? 1.35f : 1.4f;
		}
		else if (Id == TEXT("happy_ghast"))
		{
			const FVector Look = Rider->GetLookDir();
			const float F = Rider->Input.Forward;
			Vel += (Look * F * 0.08 + FVector(0, 0, Rider->Input.bJump ? 0.05 : 0.0) - Vel) * 0.1;
			MoveForward = 0.f;
		}
		else if (Id == TEXT("nautilus"))
		{
			const FVector Look = Rider->GetLookDir();
			if (bInWater) Vel += (Look * Rider->Input.Forward * 0.15 - Vel) * 0.1;
			MoveForward = bInWater ? 0.f : Rider->Input.Forward * 0.3f;
		}
		else
		{
			MoveForward = Rider->Input.Forward;
			MoveStrafe = -Rider->Input.Strafe * 0.5f;
			if (Rider->Input.bJump && bOnGround && Id != TEXT("llama") && Id != TEXT("trader_llama")) { Vel.Z = JumpPower; bJumping = false; }
			if (Id == TEXT("camel") && Rider->Input.bSprint && SpecialTimer <= 0 && bOnGround)
			{
				// camel dash
				const FVector F = FRotator(0, Yaw, 0).Vector();
				Vel += F * 1.2 + FVector(0, 0, 0.35);
				SpecialTimer = 55;
				PlaySound(TEXT("camel_dash"), 1.f, 1.f);
			}
		}
		if (SpecialTimer > 0) --SpecialTimer;
		Super::AIStep();
		return;
	}
	if (Rider && Def->AI == EMCMobAI::Mount && !bTamed && (Id == TEXT("horse") || Id == TEXT("donkey") || Id == TEXT("mule") || Id == TEXT("llama") || Id == TEXT("trader_llama") || Id == TEXT("camel")))
	{
		// taming by riding: random chance based on temper, otherwise buck the rider
		if (Rand().NextInt(50) == 0)
		{
			if (Rand().NextInt(100) < Temper + 5) { bTamed = true; Owner = Rider; World->SpawnParticles(TEXT("heart"), Pos + FVector(0, 0, Height), 7, 0.5f); PlaySound(SoundFor(Def, TEXT("ambient")), 1.f, 1.2f); }
			else { Temper = FMath::Min(100, Temper + 5); Rider->StopRiding(); PlaySound(SoundFor(Def, TEXT("angry")), 1.f, 1.f); Vel.Z = 0.3; }
		}
		MoveForward = 0.f;
		Super::AIStep();
		return;
	}

	switch (Def->AI)
	{
	case EMCMobAI::Animal: TickAnimalAI(); break;
	case EMCMobAI::Zombie: TickZombieAI(); break;
	case EMCMobAI::Skeleton: TickSkeletonAI(); break;
	case EMCMobAI::Creeper: TickCreeperAI(); break;
	case EMCMobAI::Spider: TickSpiderAI(); break;
	case EMCMobAI::Enderman: TickEndermanAI(); break;
	case EMCMobAI::Slime: TickSlimeAI(); break;
	case EMCMobAI::Ghast: TickGhastAI(); break;
	case EMCMobAI::Blaze: TickBlazeAI(); break;
	case EMCMobAI::Flyer: TickFlyerAI(); break;
	case EMCMobAI::Swimmer: TickSwimmerAI(); break;
	case EMCMobAI::Villager: TickVillagerAI(); break;
	case EMCMobAI::Golem: TickGolemAI(); break;
	case EMCMobAI::Tameable: TickTameableAI(); break;
	case EMCMobAI::Mount: TickAnimalAI(); break;
	case EMCMobAI::Illager: TickIllagerAI(); break;
	case EMCMobAI::Shulker: TickShulkerAI(); break;
	case EMCMobAI::Warden: TickWardenAI(); break;
	case EMCMobAI::Silverfish: TickZombieAI(); break;
	default: MoveForward = 0.f; break;
	}
	TickSpecialAbilities();
	if (bInWater && !(Def->bSwimmer || Def->bAmphibious || Def->bUndead) && Def->AI != EMCMobAI::Golem) bJumping = true; // float
	if (Def->bSwimmer && bInWater) { Travel(0.f, 0.f, 0.f); return; }
	Super::AIStep();
}

// ---------------------------------------------------------------------------------------------------------------------
// Visuals

void AMCMob::UpdateVisual(float Alpha, float DeltaSeconds)
{
	// animation budget: a mob that was not on screen last frame only keeps its position fresh (every 4th frame,
	// so its bounds still bring it back into view) and skips the pose; bosses always animate
	const bool bOnScreen = !Rig || Def == nullptr || Def->bBoss || Rig->IsOnScreen(0.25f);
	if (!bOnScreen)
	{
		if ((++OffscreenFrames & 3) != 0) return;
	}
	else OffscreenFrames = 0;
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	if (!bOnScreen) return;
	SetActorRotation(FRotator::ZeroRotator);
	const float BYaw = FMath::Lerp(PrevBodyYaw, PrevBodyYaw + FMath::FindDeltaAngleDegrees(PrevBodyYaw, BodyYaw), Alpha);
	float ExtraPitch = 0.f;
	if (Def && (Def->bSwimmer || Def->AI == EMCMobAI::Flyer)) ExtraPitch = -Pitch * 0.6f;
	VisualRoot->SetWorldRotation(FRotator(ExtraPitch, BYaw, 0.f));
	AnimTime += DeltaSeconds;
	FMCRigPose Pose;
	Pose.LimbSwing = LimbSwing - LimbSwingAmount * (1.f - Alpha);
	Pose.LimbAmount = FMath::Lerp(PrevLimbSwingAmount, LimbSwingAmount, Alpha);
	Pose.Age = Age + Alpha;
	Pose.HeadYaw = FMath::FindDeltaAngleDegrees(BYaw, Yaw);
	Pose.HeadPitch = Pitch;
	Pose.Attack = AttackAnim;
	Pose.bBaby = IsBaby();
	Pose.bSitting = bSitting;
	Pose.bSwimming = bInWater && Def && !Def->bSwimmer && Def->bAmphibious;
	Pose.bAggressive = Target.IsValid() && Def && (Def->AI == EMCMobAI::Zombie);
	Pose.bFlying = Def && Def->bFlying;
	Pose.Hurt = HurtTime > 0 ? HurtTime / 10.f : 0.f;
	Pose.Death = DeathTime > 0 ? FMath::Min(1.f, (DeathTime + Alpha) / 20.f) : 0.f;
	if (Def)
	{
		const FName Id = Def->Id;
		if (Id == TEXT("creeper")) Pose.Special = FMath::Lerp((float)PrevSwellTime, (float)SwellTime, Alpha) / 30.f;
		else if (Def->bFlying || Def->AI == EMCMobAI::Flyer) Pose.Special = 1.f;
		else if (Id == TEXT("chicken")) Pose.Special = bOnGround ? 0.f : 1.f;
		else if (Id == TEXT("shulker")) Pose.Special = SubState / 100.f;
		else if (Def->AI == EMCMobAI::Slime) Pose.Special = FMath::Clamp((float)(Vel.Z * 3.0), -1.f, 1.f);
		else if (Id == TEXT("ghast")) Pose.Special = ChargeTime > 10 ? 1.f : 0.f;
		else if (Id == TEXT("warden")) Pose.Special = SpecialTimer > 0 ? 1.f : 0.f;
		else if (Id == TEXT("wolf") || Id == TEXT("cat")) Pose.Special = bAngry ? 1.f : 0.f;
		// creeper swell grows the model
		if (Id == TEXT("creeper") && Pose.Special > 0.f)
		{
			const float S = 1.f + FMath::Sin(Pose.Special * 100.f) * Pose.Special * 0.01f + Pose.Special * Pose.Special * 0.2f;
			Rig->SetRelativeScale3D(FVector(S, S, 1.f + Pose.Special * 0.1f));
		}
	}
	Rig->ApplyPose(Pose);
	if (Def && Def->AI == EMCMobAI::Slime) Rig->SetRelativeScale3D(FVector((float)SplitSize));
	float Flash = Pose.Hurt > 0.f || Pose.Death > 0.f ? 0.6f : 0.f;
	if (Def && Def->Id == TEXT("creeper") && SwellTime > 0) Flash = FMath::Max(Flash, (FMath::Sin(Age * 1.5f) * 0.5f + 0.5f) * (SwellTime / 30.f));
	Rig->SetHurtFlash(Flash);
	float Fill, Torch;
	GetModelLight(Fill, Torch);
	Rig->SetLighting(Fill, Torch);
	const bool bInvisible = HasEffect(EMCEffect::Invisibility);
	Rig->SetVisibility(!bInvisible, true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Damage / death / loot

bool AMCMob::Hurt(const FMCDamage& D, float Amount)
{
	if (!Def || !IsAlive()) return false;
	const FName Id = Def->Id;
	// special immunities
	if (Id == TEXT("enderman") && D.bProjectile) { TeleportRandomly(32.f); return false; }
	if (Id == TEXT("creaking")) { PlaySound(TEXT("creaking_sway"), 1.f, 1.f); return false; } // only killable via its heart
	if (Id == TEXT("breeze") && D.bProjectile && D.Type != TEXT("wind_charge")) return false;
	if (Id == TEXT("shulker") && SubState < 10 && D.bProjectile) return false; // closed shell deflects arrows
	if (D.Type == TEXT("fall") && (Def->bFlying || Id == TEXT("cat") || Id == TEXT("iron_golem") || Id == TEXT("chicken"))) return false;
	if (D.Type == TEXT("drown") && Id == TEXT("drowned")) return false;
	const bool bHurt = Super::Hurt(D, Amount);
	if (!bHurt) return false;
	NoActionTime = 0;
	if (IsAlive()) PlaySound(SoundFor(Def, TEXT("hurt")), 1.f, IsBaby() ? 1.5f : 1.f);
	AMCLiving* Attacker = Cast<AMCLiving>(D.Attacker.Get());
	const bool bPlayerCreative = Attacker && Attacker->IsA<AMCPlayer>() && Cast<AMCPlayer>(Attacker)->IsCreative();
	if (Def->Category == EMCMobCategory::Creature && !Def->bNeutral && Def->AI != EMCMobAI::Golem && !bTamed) PanicTime = 60 + Rand().NextInt(40);
	if (Attacker && Attacker != this && !bPlayerCreative)
	{
		if (Def->bNeutral || Def->bHostile || Def->AI == EMCMobAI::Golem || Id == TEXT("goat") || Id == TEXT("panda") || Id == TEXT("fox") || Id == TEXT("llama") || Id == TEXT("trader_llama"))
		{
			if (!(bTamed && Owner.Get() == Attacker)) SetAngryAt(Attacker, 400 + Rand().NextInt(400));
		}
		// group anger: zombified piglins, wolves, bees, piglins
		if (Id == TEXT("zombified_piglin") || Id == TEXT("wolf") || Id == TEXT("bee") || Id == TEXT("piglin") || Id == TEXT("polar_bear") || Id == TEXT("llama"))
		{
			TArray<AMCEntity*> Near;
			World->GetEntitiesInBox(GetBox().Inflate(Id == TEXT("zombified_piglin") ? 20.0 : 10.0), Near, this);
			for (AMCEntity* E : Near)
			{
				AMCMob* M = Cast<AMCMob>(E);
				if (M && M->Def && M->Def->Id == Id && !M->bTamed) M->SetAngryAt(Attacker, 400 + Rand().NextInt(400));
			}
		}
		// villagers remember: nearby iron golems defend them
		if (Id == TEXT("villager") && Attacker->IsA<AMCPlayer>())
		{
			TArray<AMCEntity*> Near;
			World->GetEntitiesInBox(GetBox().Inflate(16.0), Near, this);
			for (AMCEntity* E : Near) if (AMCMob* M = Cast<AMCMob>(E)) if (M->Def && M->Def->Id == TEXT("iron_golem")) M->SetAngryAt(Attacker, 600);
		}
	}
	if (Id == TEXT("squid") || Id == TEXT("glow_squid"))
	{
		World->SpawnParticles(Id == TEXT("squid") ? TEXT("squid_ink") : TEXT("glow_squid_ink"), Pos + FVector(0, 0, Height * 0.5), 30, 0.6f, FVector::ZeroVector, Id == TEXT("squid") ? FColor(10, 10, 20) : FColor(40, 220, 200));
		PanicTime = 100;
	}
	if (Id == TEXT("shulker") && Health < MaxHealth * 0.5f && Rand().NextInt(4) == 0) TeleportRandomly(8.f);
	if (Id == TEXT("pufferfish") && Attacker) { FMCEffectInstance E; E.Effect = EMCEffect::Poison; E.Duration = 60 * (int32)(Game ? Game->Difficulty : EMCDifficulty::Normal); Attacker->AddEffect(E); }
	return true;
}

void AMCMob::Die(const FMCDamage& D)
{
	if (DeathTime > 0 || !Def) { Super::Die(D); return; }
	PlaySound(SoundFor(Def, TEXT("death")), 1.f, IsBaby() ? 1.5f : 1.f);
	Super::Die(D);
	const FName Id = Def->Id;
	// slimes split into smaller ones
	if (Def->AI == EMCMobAI::Slime && SplitSize > 1 && Game)
	{
		const int32 N = 2 + Rand().NextInt(3);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Off((i % 2 - 0.5f) * SplitSize * 0.25f, (i / 2 - 0.5f) * SplitSize * 0.25f, 0.5);
			if (AMCMob* M = Cast<AMCMob>(Game->SpawnMob(World, Id, Pos + Off, false)))
			{
				M->SplitSize = SplitSize / 2;
				M->Width = M->Height = 0.52f * M->SplitSize;
				M->MaxHealth = (float)(M->SplitSize * M->SplitSize);
				M->Health = M->MaxHealth;
				M->MoveSpeed = 0.2f + 0.1f * M->SplitSize;
				M->bPersistent = bPersistent;
			}
		}
	}
	// villager -> zombie villager conversion
	if (Id == TEXT("villager") && D.Attacker.IsValid() && D.Attacker->IsA<AMCMob>() && Game)
	{
		const AMCMob* K = Cast<AMCMob>(D.Attacker.Get());
		const bool bZombie = K->Def && (K->Def->Id == TEXT("zombie") || K->Def->Id == TEXT("drowned") || K->Def->Id == TEXT("husk") || K->Def->Id == TEXT("zombie_villager"));
		const float Chance = Game->Difficulty == EMCDifficulty::Hard ? 1.f : (Game->Difficulty == EMCDifficulty::Normal ? 0.5f : 0.f);
		if (bZombie && Rand().NextFloat() < Chance)
		{
			if (AMCMob* Z = Cast<AMCMob>(Game->SpawnMob(World, TEXT("zombie_villager"), Pos, false))) { Z->bPersistent = true; Z->Profession = Profession; }
			Discard();
		}
	}
	// ender dragon egg / boss handled in boss classes; tamed pets message
	if (bTamed && Owner.IsValid() && Owner->IsA<AMCPlayer>() && Game) Game->AddChat(LastDeathMessage);
}

void AMCMob::DropLoot(const FMCDamage& D, bool bPlayerKill)
{
	if (!Def || !World) return;
	AMCLiving* Killer = Cast<AMCLiving>(D.Attacker.Get());
	const int32 Looting = Killer ? Killer->MainHandConst().GetEnchant(EMCEnchant::Looting) : 0;
	TArray<FMCItemStack> Drops;
	if (!(IsBaby() && Def->Category == EMCMobCategory::Creature))
	{
		MCLoot::GetMobDrops(Def->Id, Variant, Looting, bPlayerKill, IsOnFire(), IsBaby(), Rand(), Drops);
	}
	if (Def->Id == TEXT("sheep") && !bSheared && !IsBaby())
	{
		static const TCHAR* Wools[16] = { TEXT("white_wool"), TEXT("orange_wool"), TEXT("magenta_wool"), TEXT("light_blue_wool"), TEXT("yellow_wool"), TEXT("lime_wool"), TEXT("pink_wool"), TEXT("gray_wool"),
			TEXT("light_gray_wool"), TEXT("cyan_wool"), TEXT("purple_wool"), TEXT("blue_wool"), TEXT("brown_wool"), TEXT("green_wool"), TEXT("red_wool"), TEXT("black_wool") };
		Drops.RemoveAll([](const FMCItemStack& S) { return S.Item().Name.ToString().EndsWith(TEXT("_wool")); });
		Drops.Add(FMCItemStack::Of(Wools[DyeColor & 15], 1));
	}
	// equipment
	for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s)
	{
		const FMCItemStack& E = Equipment[s];
		if (E.IsEmpty()) continue;
		if (Rand().NextFloat() < DropChances[s] + Looting * 0.01f)
		{
			FMCItemStack C = E.Copy();
			if (C.IsDamageable() && DropChances[s] < 1.f) C.Damage = FMath::Clamp(C.Item().MaxDamage - 1 - Rand().NextInt(FMath::Max(1, C.Item().MaxDamage / 2)), 0, C.Item().MaxDamage - 1);
			Drops.Add(C);
		}
	}
	if (bSaddled) Drops.Add(FMCItemStack::Of(TEXT("saddle"), 1));
	if (bHasChest) Drops.Add(FMCItemStack::Of(TEXT("chest"), 1));
	if (bHarnessed) Drops.Add(FMCItemStack::Of(TEXT("white_harness"), 1));
	for (FMCItemStack& S : MobInventory.Slots) if (!S.IsEmpty()) { Drops.Add(S); S.Clear(); }
	if (CarriedBlock) { const FMCItemId CI = FMCItems::ForBlock(FMCBlocks::BlockOf(CarriedBlock)); if (CI) Drops.Add(FMCItemStack(CI, 1)); CarriedBlock = 0; }
	// music disc when a skeleton kills a creeper
	if (Def->Id == TEXT("creeper") && D.Attacker.IsValid() && D.Attacker->IsA<AMCMob>() && Cast<AMCMob>(D.Attacker.Get())->Def && Cast<AMCMob>(D.Attacker.Get())->Def->AI == EMCMobAI::Skeleton)
		Drops.Add(FMCItemStack::Of(Rand().NextBool() ? TEXT("music_disc_opus55") : TEXT("music_disc_voxel"), 1));
	// charged creeper explosions drop heads
	if (D.bExplosion && D.Direct.IsValid() && D.Direct->IsA<AMCMob>() && Cast<AMCMob>(D.Direct.Get())->bCharged)
	{
		const FName Id = Def->Id;
		const TCHAR* Head = Id == TEXT("zombie") ? TEXT("zombie_head") : (Id == TEXT("skeleton") ? TEXT("skeleton_skull") : (Id == TEXT("creeper") ? TEXT("creeper_head") : (Id == TEXT("piglin") ? TEXT("piglin_head") : (Id == TEXT("wither_skeleton") ? TEXT("wither_skeleton_skull") : nullptr))));
		if (Head) Drops.Add(FMCItemStack::Of(Head, 1));
	}
	for (const FMCItemStack& S : Drops)
	{
		if (S.IsEmpty()) continue;
		World->SpawnItem(Pos + FVector(0, 0, Height * 0.5), S, true, 0.5f);
	}
}

int32 AMCMob::GetXPReward() const
{
	if (!Def) return 0;
	if (IsBaby() && Def->Category == EMCMobCategory::Creature) return 0;
	int32 XP = Def->XP;
	if (Def->Category == EMCMobCategory::Creature) XP = 1 + Rand().NextInt(3);
	if (Def->AI == EMCMobAI::Slime) XP = SplitSize;
	if (Def->Category == EMCMobCategory::Monster)
	{
		for (int32 s = 0; s < (int32)EMCEquipSlot::Count; ++s) if (!Equipment[s].IsEmpty()) XP += 1 + Rand().NextInt(3);
		if (IsBaby()) XP = FMath::RoundToInt(XP * 2.5f);
	}
	return XP;
}

// ---------------------------------------------------------------------------------------------------------------------
// Breeding, taming, anger

bool AMCMob::IsBreedItem(const FMCItemStack& S) const
{
	if (!Def || S.IsEmpty()) return false;
	const FMCItem& I = S.Item();
	for (const FName& B : Def->BreedItems)
	{
		const FString BS = B.ToString();
		if (BS.StartsWith(TEXT("#"))) { if (I.HasTag(FName(*BS.Mid(1)))) return true; }
		else if (I.Name == B) return true;
	}
	if (Def->Id == TEXT("bee") && (I.HasTag(TEXT("flowers")) || I.HasTag(TEXT("small_flowers")))) return true;
	return false;
}

bool AMCMob::IsTemptedBy(const AMCPlayer* P) const
{
	if (!P || !Def) return false;
	const FMCItemStack& M = P->HeldConst();
	const FMCItemStack& O = P->Inventory.Slots[MCInv::Offhand];
	if (IsBreedItem(M) || IsBreedItem(O)) return true;
	const FName Id = Def->Id;
	auto Holds = [&](const TCHAR* N) { return (!M.IsEmpty() && M.Item().Name == N) || (!O.IsEmpty() && O.Item().Name == N); };
	if (Id == TEXT("pig") && Holds(TEXT("carrot_on_a_stick"))) return true;
	if (Id == TEXT("strider") && Holds(TEXT("warped_fungus_on_a_stick"))) return true;
	if (Id == TEXT("happy_ghast") && Holds(TEXT("snowball"))) return true;
	return false;
}

void AMCMob::SetAngryAt(AMCLiving* Who, int32 Ticks)
{
	if (!Who || Who == this || !Def) return;
	if (const AMCPlayer* P = Cast<AMCPlayer>(Who)) if (P->IsCreative() || P->IsSpectator()) return;
	if (Game && Game->Difficulty == EMCDifficulty::Peaceful && Who->IsA<AMCPlayer>() && Def->Category == EMCMobCategory::Monster) return;
	Target = Who;
	bAngry = true;
	AngerTime = Ticks;
	if (Def->Id == TEXT("enderman")) PlaySound(TEXT("enderman_stare"), 1.f, 1.f);
}

void AMCMob::Breed(AMCMob* Partner)
{
	if (!Partner || !Game || !Def) return;
	InLove = 0; Partner->InLove = 0;
	GrowAge = 6000; Partner->GrowAge = 6000;
	FName ChildId = Def->Id;
	if ((Def->Id == TEXT("horse") && Partner->Def->Id == TEXT("donkey")) || (Def->Id == TEXT("donkey") && Partner->Def->Id == TEXT("horse"))) ChildId = TEXT("mule");
	const int32 Count = Def->Id == TEXT("turtle") || Def->Id == TEXT("frog") ? 0 : 1;
	if (Def->Id == TEXT("turtle"))
	{
		// turtles lay eggs on sand: handled by placing an egg block nearby
		const FMCBlockPos P = BlockPos();
		if (FMCBlocks::GetByState(World->GetState(P.Down())).Has(MCB_Sand) && World->GetState(P) == 0) World->SetState(P, FMCBlocks::FindState(TEXT("turtle_egg"), (uint8)Rand().NextInt(4)));
	}
	if (Def->Id == TEXT("frog"))
	{
		// frogspawn on water
		for (int32 dx = -2; dx <= 2; ++dx) for (int32 dy = -2; dy <= 2; ++dy)
		{
			const FMCBlockPos P(MC::FloorToInt(Pos.X) + dx, MC::FloorToInt(Pos.Y) + dy, MC::FloorToInt(Pos.Z));
			if (FMCBlocks::Info(World->GetState(P.Down())).Block == FMCBlocks::C.WaterId && World->GetState(P) == 0) { World->SetState(P, FMCBlocks::FindState(TEXT("frogspawn"))); dx = 3; break; }
		}
	}
	for (int32 i = 0; i < Count; ++i)
	{
		if (AMCMob* Baby = Cast<AMCMob>(Game->SpawnMob(World, ChildId, (Pos + Partner->Pos) * 0.5, false, Rand().NextBool() ? Variant : Partner->Variant)))
		{
			Baby->SetBaby(true);
			Baby->bPersistent = true;
			if (bTamed) { Baby->bTamed = true; Baby->Owner = Owner; }
			if (Def->Id == TEXT("sheep"))
			{
				Baby->DyeColor = Rand().NextBool() ? DyeColor : Partner->DyeColor;
				Baby->Color = SheepColors[Baby->DyeColor & 15];
				Baby->ApplyVariantVisuals();
			}
		}
	}
	World->SpawnParticles(TEXT("heart"), Pos + FVector(0, 0, Height), 7, 0.6f);
	World->SpawnXP(Pos, 1 + Rand().NextInt(7));
	if (AMCPlayer* P = LoveCause.Get()) { (void)P; }
}

// ---------------------------------------------------------------------------------------------------------------------
// Interaction

bool AMCMob::Interact(AMCPlayer* Player, bool bOffHand)
{
	if (!Def || !Player || !IsAlive() || !World) return false;
	FMCItemStack& H = bOffHand ? Player->Inventory.Slots[MCInv::Offhand] : Player->Held();
	const FName Item = H.IsEmpty() ? NAME_None : H.Item().Name;
	const FName Id = Def->Id;
	auto Consume = [&]() { if (!Player->IsCreative()) { H.Count -= 1; if (H.Count <= 0) H.Clear(); } };

	// name tags & leads work on almost everything
	if (Item == TEXT("name_tag") && H.Extra.IsValid() && !H.Extra->CustomName.IsEmpty())
	{
		CustomName = H.Extra->CustomName;
		bPersistent = true;
		Consume();
		return true;
	}
	if (Item == TEXT("lead") && !LeashHolder.IsValid() && Def->Category != EMCMobCategory::Monster && Def->AI != EMCMobAI::Villager)
	{
		LeashHolder = Player;
		Consume();
		PlaySound(TEXT("lead_tied"), 1.f, 1.f);
		return true;
	}
	if (LeashHolder.Get() == Player && H.IsEmpty()) { LeashHolder.Reset(); Player->GiveItem(FMCItemStack::Of(TEXT("lead"), 1)); return true; }

	// feeding / breeding
	if (IsBreedItem(H) && Def->bCanBreed)
	{
		if (IsBaby())
		{
			GrowAge = FMath::Min(0, GrowAge + (-GrowAge) / 10);
			Consume();
			World->SpawnParticles(TEXT("happy_villager"), Pos + FVector(0, 0, Height), 5, 0.4f);
			return true;
		}
		const bool bNeedsTame = Def->AI == EMCMobAI::Tameable || (Def->AI == EMCMobAI::Mount && Id != TEXT("strider") && Id != TEXT("camel"));
		if (GrowAge == 0 && InLove <= 0 && (!bNeedsTame || bTamed))
		{
			InLove = 600;
			LoveCause = Player;
			Consume();
			if (Health < GetMaxHealth()) Heal(2.f);
			PlaySound(SoundFor(Def, TEXT("eat")), 1.f, 1.f);
			return true;
		}
		if (Health < GetMaxHealth() && bTamed) { Heal(Id == TEXT("wolf") ? 4.f : 2.f); Consume(); return true; }
	}

	// taming
	if (Id == TEXT("wolf") && !bTamed && !bAngry && Item == TEXT("bone"))
	{
		Consume();
		if (Rand().NextInt(3) == 0) { bTamed = true; Owner = Player; bSitting = true; MaxHealth = 40.f; Health = 40.f; World->SpawnParticles(TEXT("heart"), Pos + FVector(0, 0, Height), 7, 0.5f); }
		else World->SpawnParticles(TEXT("smoke"), Pos + FVector(0, 0, Height), 7, 0.5f);
		return true;
	}
	if ((Id == TEXT("cat") || Id == TEXT("ocelot")) && !bTamed && (Item == TEXT("cod") || Item == TEXT("salmon")))
	{
		Consume();
		if (Rand().NextInt(3) == 0)
		{
			if (Id == TEXT("cat")) { bTamed = true; Owner = Player; bSitting = true; }
			else bTrusting = true;
			World->SpawnParticles(TEXT("heart"), Pos + FVector(0, 0, Height), 7, 0.5f);
		}
		else World->SpawnParticles(TEXT("smoke"), Pos + FVector(0, 0, Height), 7, 0.5f);
		return true;
	}
	if (Id == TEXT("parrot") && !bTamed && H.Item().Kind == EMCItemKind::Seeds)
	{
		Consume();
		if (Rand().NextInt(10) == 0) { bTamed = true; Owner = Player; World->SpawnParticles(TEXT("heart"), Pos + FVector(0, 0, Height), 7, 0.5f); }
		return true;
	}
	if (bTamed && Owner.Get() == Player && (Id == TEXT("wolf") || Id == TEXT("cat") || Id == TEXT("parrot")) && !IsBreedItem(H))
	{
		if (Id == TEXT("wolf") && H.Item().Kind == EMCItemKind::Dye) { DyeColor = H.Item().DyeColor; Consume(); return true; }
		if (Id == TEXT("wolf") && Item == TEXT("wolf_armor")) { Equipment[(int32)EMCEquipSlot::Chest] = H.Copy(); Consume(); PlaySound(TEXT("armor_equip"), 1.f, 1.f); return true; }
		bSitting = !bSitting;
		StopNavigation();
		return true;
	}

	// farm products
	if ((Id == TEXT("cow") || Id == TEXT("goat") || Id == TEXT("mooshroom")) && Item == TEXT("bucket") && !IsBaby())
	{
		Player->ReplaceHeld(FMCItemStack::Of(TEXT("milk_bucket"), 1), bOffHand);
		PlaySound(Id == TEXT("goat") ? TEXT("goat_milk") : TEXT("cow_milk"), 1.f, 1.f);
		return true;
	}
	if (Id == TEXT("mooshroom") && Item == TEXT("bowl") && !IsBaby())
	{
		Player->ReplaceHeld(FMCItemStack::Of(TEXT("mushroom_stew"), 1), bOffHand);
		PlaySound(TEXT("mooshroom_milk"), 1.f, 1.f);
		return true;
	}
	if (Item == TEXT("shears") && !IsBaby())
	{
		if (Id == TEXT("sheep") && !bSheared)
		{
			bSheared = true;
			static const TCHAR* Wools[16] = { TEXT("white_wool"), TEXT("orange_wool"), TEXT("magenta_wool"), TEXT("light_blue_wool"), TEXT("yellow_wool"), TEXT("lime_wool"), TEXT("pink_wool"), TEXT("gray_wool"),
				TEXT("light_gray_wool"), TEXT("cyan_wool"), TEXT("purple_wool"), TEXT("blue_wool"), TEXT("brown_wool"), TEXT("green_wool"), TEXT("red_wool"), TEXT("black_wool") };
			const int32 N = 1 + Rand().NextInt(3);
			for (int32 i = 0; i < N; ++i) World->SpawnItem(Pos + FVector(0, 0, 1), FMCItemStack::Of(Wools[DyeColor & 15], 1));
			PlaySound(TEXT("sheep_shear"), 1.f, 1.f);
			Player->DamageHeld(1, bOffHand);
			ApplyVariantVisuals();
			return true;
		}
		if (Id == TEXT("mooshroom"))
		{
			for (int32 i = 0; i < 5; ++i) World->SpawnItem(Pos + FVector(0, 0, Height), FMCItemStack::Of(Variant == 1 ? TEXT("brown_mushroom") : TEXT("red_mushroom"), 1));
			if (Game) if (AMCMob* Cow = Cast<AMCMob>(Game->SpawnMob(World, TEXT("cow"), Pos, false))) { Cow->Yaw = Yaw; Cow->bPersistent = true; }
			World->SpawnParticles(TEXT("explosion"), Pos + FVector(0, 0, Height * 0.5), 1, 0.f);
			Player->DamageHeld(1, bOffHand);
			Discard();
			return true;
		}
		if (Id == TEXT("snow_golem") && !bSheared) { bSheared = true; World->SpawnItem(Pos + FVector(0, 0, Height), FMCItemStack::Of(TEXT("carved_pumpkin"), 1)); Player->DamageHeld(1, bOffHand); return true; }
		if (Id == TEXT("bogged") && !bSheared) { bSheared = true; World->SpawnItem(Pos + FVector(0, 0, Height), FMCItemStack::Of(TEXT("brown_mushroom"), 1)); Player->DamageHeld(1, bOffHand); return true; }
	}
	if (Id == TEXT("sheep") && H.Item().Kind == EMCItemKind::Dye && !bSheared)
	{
		DyeColor = H.Item().DyeColor & 15;
		Color = SheepColors[DyeColor];
		ApplyVariantVisuals();
		Consume();
		return true;
	}
	if (Id == TEXT("iron_golem") && Item == TEXT("iron_ingot") && Health < GetMaxHealth())
	{
		Heal(25.f);
		Consume();
		PlaySound(TEXT("iron_golem_repair"), 1.f, 1.f);
		return true;
	}
	if ((Id == TEXT("axolotl") || Id == TEXT("cod") || Id == TEXT("salmon") || Id == TEXT("pufferfish") || Id == TEXT("tropical_fish") || Id == TEXT("tadpole")) && Item == TEXT("water_bucket"))
	{
		Player->ReplaceHeld(FMCItemStack::Of(FName(*(Id.ToString() + TEXT("_bucket"))), 1), bOffHand);
		PlaySound(TEXT("bucket_fill_fish"), 1.f, 1.f);
		Discard();
		return true;
	}
	if (Id == TEXT("allay"))
	{
		if (!H.IsEmpty() && GetItem(EMCEquipSlot::MainHand).IsEmpty())
		{
			GetItem(EMCEquipSlot::MainHand) = H.Split(1);
			if (Player->IsCreative()) H.Count += 1;
			Owner = Player;
			PlaySound(TEXT("allay_item_given"), 1.f, 1.f);
			return true;
		}
		if (H.IsEmpty() && !GetItem(EMCEquipSlot::MainHand).IsEmpty())
		{
			Player->GiveItem(GetItem(EMCEquipSlot::MainHand));
			GetItem(EMCEquipSlot::MainHand).Clear();
			PlaySound(TEXT("allay_item_taken"), 1.f, 1.f);
			return true;
		}
	}

	// saddles, chests, harnesses
	const bool bSaddleable = Id == TEXT("pig") || Id == TEXT("strider") || Id == TEXT("horse") || Id == TEXT("donkey") || Id == TEXT("mule") || Id == TEXT("camel") || Id == TEXT("skeleton_horse") || Id == TEXT("zombie_horse") || Id == TEXT("nautilus");
	if (Item == TEXT("saddle") && bSaddleable && !bSaddled && !IsBaby() && (bTamed || Id == TEXT("pig") || Id == TEXT("strider") || Id == TEXT("nautilus") || Id == TEXT("camel")))
	{
		bSaddled = true;
		Consume();
		PlaySound(TEXT("saddle_equip"), 1.f, 1.f);
		return true;
	}
	if (Item == TEXT("chest") && (Id == TEXT("donkey") || Id == TEXT("mule") || Id == TEXT("llama") || Id == TEXT("trader_llama")) && bTamed && !bHasChest)
	{
		bHasChest = true;
		if (MobInventory.Num() < 15) MobInventory.Init(15);
		Consume();
		PlaySound(TEXT("donkey_chest"), 1.f, 1.f);
		return true;
	}
	if (Id == TEXT("happy_ghast") && Item.ToString().EndsWith(TEXT("_harness")) && !bHarnessed)
	{
		bHarnessed = true;
		Consume();
		PlaySound(TEXT("harness_equip"), 1.f, 1.f);
		return true;
	}
	if (Id == TEXT("happy_ghast") && Item == TEXT("snowball") && IsBaby()) { GrowAge = FMath::Min(0, GrowAge + 1200); Consume(); return true; }

	// trading
	if ((Id == TEXT("villager") || Id == TEXT("wandering_trader")) && !IsBaby())
	{
		if (Trades.Num() == 0) GenerateTrades();
		if (Trades.Num() == 0 || Profession == TEXT("nitwit") || Profession == TEXT("none"))
		{
			PlaySound(TEXT("villager_no"), 1.f, 1.f);
			return true;
		}
		Player->OpenMenu(MakeShared<FMCMerchantMenu>(Player, this));
		PlaySound(TEXT("villager_trade"), 1.f, 1.f);
		return true;
	}

	// riding
	const bool bRideable = (bSaddleable && (bSaddled || (Def->AI == EMCMobAI::Mount && (Id == TEXT("horse") || Id == TEXT("donkey") || Id == TEXT("mule") || Id == TEXT("camel") || Id == TEXT("llama") || Id == TEXT("trader_llama") || Id == TEXT("skeleton_horse") || Id == TEXT("zombie_horse")))))
		|| (Id == TEXT("happy_ghast") && bHarnessed) || Id == TEXT("llama") || Id == TEXT("trader_llama");
	if (bRideable && !IsBaby() && Passengers.Num() == 0 && !Player->bSneaking && !Player->IsPassenger())
	{
		if (bHasChest && Player->bSneaking) return false;
		Player->StartRiding(this);
		return true;
	}
	return false;
}

FString AMCMob::GetDisplayName() const
{
	if (!CustomName.IsEmpty()) return CustomName;
	if (!Def) return Super::GetDisplayName();
	if (Def->Id == TEXT("villager") && !Profession.IsNone() && Profession != TEXT("none"))
	{
		FString P = Profession.ToString();
		if (P.Len() > 0) P[0] = FChar::ToUpper(P[0]);
		return P;
	}
	return Def->Name;
}

// ---------------------------------------------------------------------------------------------------------------------
// Misc overrides

bool AMCMob::IsSensitiveToWater() const
{
	return Def && (Def->Id == TEXT("enderman") || Def->Id == TEXT("blaze") || Def->Id == TEXT("snow_golem") || Def->Id == TEXT("strider"));
}

void AMCMob::OnLightning()
{
	if (!Def || !Game || !World) { Super::OnLightning(); return; }
	const FName Id = Def->Id;
	auto Convert = [&](const TCHAR* To)
	{
		if (AMCMob* M = Cast<AMCMob>(Game->SpawnMob(World, FName(To), Pos, false)))
		{
			M->Yaw = Yaw; M->bPersistent = true; M->CustomName = CustomName;
			Discard();
		}
	};
	if (Id == TEXT("creeper")) { bCharged = true; ApplyVariantVisuals(); SetOnFire(8); return; }
	if (Id == TEXT("pig")) { Convert(TEXT("zombified_piglin")); return; }
	if (Id == TEXT("villager")) { Convert(TEXT("witch")); return; }
	if (Id == TEXT("mooshroom")) { Variant = 1 - Variant; ApplyVariantVisuals(); return; }
	Super::OnLightning();
}

bool AMCMob::CanCollideWith(const AMCEntity* Other) const
{
	return Def && Def->Id == TEXT("shulker") && IsAlive();
}

void AMCMob::OnDeathAnimationFinished()
{
	Super::OnDeathAnimationFinished();
}

FVector AMCMob::GetPassengerOffset(const AMCEntity* Passenger) const
{
	if (!Def) return FVector(0, 0, Height);
	const FName Id = Def->Id;
	double Z = Height * 0.75;
	if (Id == TEXT("pig")) Z = 0.6;
	else if (Id == TEXT("strider")) Z = 1.4;
	else if (Id == TEXT("camel") || Id == TEXT("camel_husk")) Z = bSitting ? 0.9 : 1.9;
	else if (Id == TEXT("happy_ghast")) Z = 4.0;
	else if (Id == TEXT("spider")) Z = 0.55;
	else if (Id == TEXT("chicken")) Z = 0.55;
	else if (Def->AI == EMCMobAI::Mount) Z = Height * 0.72;
	const FVector Back = FRotator(0, BodyYaw, 0).Vector() * (Id == TEXT("camel") ? -0.3 : 0.0);
	return FVector(Back.X, Back.Y, Z);
}

bool AMCMob::TeleportRandomly(float Range)
{
	if (!World || TeleportCooldown > 0) return false;
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const FVector T = Pos + FVector(Rand().FRange(-Range, Range), Rand().FRange(-Range, Range), Rand().FRange(-Range * 0.5f, Range * 0.5f));
		if (TeleportTowards(T)) return true;
	}
	return false;
}

bool AMCMob::TeleportTowards(const FVector& Dest)
{
	if (!World) return false;
	const int32 X = MC::FloorToInt(Dest.X), Y = MC::FloorToInt(Dest.Y);
	int32 Z = MC::FloorToInt(Dest.Z);
	for (int32 i = 0; i < 24 && Z > MC::MinZ; ++i, --Z)
	{
		const FMCState Below = World->GetState(FMCBlockPos(X, Y, Z - 1));
		if (!FMCBlocks::IsSolid(Below) || FMCBlocks::IsFluid(Below)) continue;
		const FVector Cand(X + 0.5, Y + 0.5, Z);
		if (!World->IsRegionFree(GetBoxAt(Cand))) continue;
		if (World->IsInFluid(GetBoxAt(Cand), FMCBlocks::C.WaterId) && IsSensitiveToWater()) continue;
		World->SpawnParticles(TEXT("portal"), Pos + FVector(0, 0, Height * 0.5), 32, Width, FVector::ZeroVector, FColor(150, 60, 220));
		World->PlaySound(Def && Def->Id == TEXT("shulker") ? TEXT("shulker_teleport") : TEXT("enderman_teleport"), Pos, 1.f, 1.f);
		TeleportTo(Cand);
		World->PlaySound(Def && Def->Id == TEXT("shulker") ? TEXT("shulker_teleport") : TEXT("enderman_teleport"), Pos, 1.f, 1.f);
		TeleportCooldown = 10;
		StopNavigation();
		return true;
	}
	return false;
}

void AMCMob::Explode()
{
	if (!World || bRemoved) return;
	const float Power = (bCharged ? 6.f : 3.f);
	World->Explode(Pos + FVector(0, 0, Height * 0.5), Power, false, !Game || Game->Rules.bMobGriefing, this);
	// lingering effects from potions the creeper had
	if (Effects.Num() > 0 && Game)
	{
		if (AMCAreaCloud* C = Game->SpawnEntity<AMCAreaCloud>(World, Pos))
		{
			C->Effect = Effects[0].Effect; C->EffectDuration = Effects[0].Duration / 2; C->Amplifier = Effects[0].Amplifier; C->Radius = 2.5f;
			C->Color = MCEffects::Color(C->Effect);
		}
	}
	Health = 0.f;
	DeathTime = 20;
	Discard();
}

// ---------------------------------------------------------------------------------------------------------------------
// Persistence

void AMCMob::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	int32 Version = 2;
	Ar << Version;
	if (Version >= 2) Ar << bNoAI;
	Ar << Variant << GrowAge << InLove << bTamed << bSitting << bSaddled << bSheared << bCharged << bAngry << AngerTime;
	Ar << Color << DyeColor << SplitSize << HomePos << bHasHome << CarriedBlock << Profession << VillagerLevel << VillagerXP;
	Ar << bHasChest << bHarnessed << bTrusting << TameAttempts << Temper << JumpPower << MoveSpeed << MaxHealth << bNaturalSpawn;
	MobInventory.Serialize(Ar);
	int32 NumTrades = Trades.Num();
	Ar << NumTrades;
	if (Ar.IsLoading()) Trades.SetNum(FMath::Clamp(NumTrades, 0, 64));
	for (FMCTrade& T : Trades) Ar << T.CostA << T.CostB << T.Result << T.Uses << T.MaxUses << T.XP << T.PriceMultiplier;
	bool bOwnedByPlayer = Owner.IsValid() && Owner->IsA<AMCPlayer>();
	Ar << bOwnedByPlayer;
	if (Ar.IsLoading())
	{
		if (bOwnedByPlayer && Game && Game->Player) Owner = Game->Player;
		if (Def && Def->AI == EMCMobAI::Slime) { Width = Height = 0.52f * SplitSize; }
		ApplyVariantVisuals();
	}
}
