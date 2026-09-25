// Mob AI archetypes (part 2): flyers, swimmers, villagers & trades, golems, pets, illagers, shulkers, the warden.
#include "Game/MCMob.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCEntities.h"
#include "Items/MCLoot.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"

namespace
{
	bool ValidTarget(const AMCLiving* T, const AMCMob* Self)
	{
		if (!T || !T->IsAlive() || T->bRemoved || T->World != Self->World) return false;
		if (const AMCPlayer* P = Cast<AMCPlayer>(T)) if (P->IsCreative() || P->IsSpectator()) return false;
		return true;
	}
	bool IsHostileMob(const AMCLiving* L)
	{
		const AMCMob* M = Cast<AMCMob>(L);
		return M && M->Def && M->Def->Category == EMCMobCategory::Monster && M->Def->Id != TEXT("creeper") && M->IsAlive();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Flyers: bats, parrots, allays, bees, vexes, phantoms, happy ghasts

void AMCMob::TickFlyerAI()
{
	if (!Def || !World) return;
	const FName Id = Def->Id;
	MoveForward = 0.f;
	if (Id == TEXT("vex") || Id == TEXT("phantom") || (Id == TEXT("bee") && bAngry)) TickCommonTargeting();
	AMCLiving* T = Target.Get();
	if (T && !ValidTarget(T, this)) { Target.Reset(); T = nullptr; }

	if (Id == TEXT("bat"))
	{
		// erratic flight, hang upside down under blocks during the day
		if (bSitting)
		{
			Vel = FVector::ZeroVector;
			if (Rand().NextInt(200) == 0 || (Game && Game->Player && FVector::DistSquared(Game->Player->Pos, Pos) < 16.0)) bSitting = false;
			return;
		}
		if (FVector::DistSquared(FlyTarget, Pos) < 4.0 || Rand().NextInt(30) == 0)
			FlyTarget = Pos + FVector(Rand().FRange(-7.f, 7.f), Rand().FRange(-7.f, 7.f), Rand().FRange(-2.f, 4.f));
		TickFlyMovement(0.1f);
		if (FMCBlocks::IsOpaque(World->GetState(BlockPos().Up(1))) && Rand().NextInt(100) == 0) bSitting = true;
		return;
	}
	if (Id == TEXT("vex") && T)
	{
		FlyTarget = T->GetEyePos() - FVector(0, 0, 0.5);
		TickFlyMovement(0.25f);
		LookAt(T->GetEyePos(), 30.f, 30.f);
		if (FVector::DistSquared(T->Pos, Pos) < 2.5) DoMeleeAttack(T);
		if (++SpecialTimer > 600 + Rand().NextInt(200)) Hurt(FMCDamage::Of(TEXT("magic")), 1.f); // vexes wither away
		return;
	}
	if (Id == TEXT("phantom"))
	{
		// circle overhead, swoop at the target
		if (T && SubState == 1)
		{
			FlyTarget = T->Pos + FVector(0, 0, 0.5);
			TickFlyMovement(0.35f);
			if (FVector::DistSquared(T->Pos, Pos) < 2.0) { DoMeleeAttack(T); SubState = 0; SpecialTimer = 60; PlaySound(TEXT("phantom_bite"), 1.f, 1.f); }
			if (bHorizontalCollision) SubState = 0;
			return;
		}
		const FVector Center = T ? T->Pos + FVector(0, 0, 12) : AimPos;
		if (!T && AimPos.IsZero()) AimPos = Pos;
		const float A = Age * 0.05f;
		FlyTarget = Center + FVector(FMath::Cos(A) * 10.f, FMath::Sin(A) * 10.f, FMath::Sin(A * 0.7f) * 2.f);
		TickFlyMovement(0.2f);
		if (T && --SpecialTimer <= 0) { SubState = 1; PlaySound(TEXT("phantom_swoop"), 1.f, 1.f); SpecialTimer = 120; }
		return;
	}
	if (Id == TEXT("bee"))
	{
		if (T)
		{
			FlyTarget = T->GetEyePos() - FVector(0, 0, 0.4);
			TickFlyMovement(0.2f);
			if (FVector::DistSquared(T->Pos, Pos) < 2.0 && DoMeleeAttack(T))
			{
				// a bee dies after stinging
				bAngry = false; Target.Reset(); SpecialTimer = 1;
				SubState = 1;
			}
			return;
		}
		if (SubState == 1 && ++SpecialTimer > 1200) { Hurt(FMCDamage::Of(TEXT("generic")), 100.f); return; }
		// pollinate: fly between flowers near the hive
		if (FVector::DistSquared(FlyTarget, Pos) < 1.0 || Rand().NextInt(80) == 0)
		{
			FlyTarget = Pos + FVector(Rand().FRange(-6.f, 6.f), Rand().FRange(-6.f, 6.f), Rand().FRange(-1.f, 2.f));
			const FMCBlockPos G(MC::FloorToInt(FlyTarget.X), MC::FloorToInt(FlyTarget.Y), MC::FloorToInt(FlyTarget.Z));
			const int32 GZ = World->FindGroundZ(G.X, G.Y, G.Z + 3);
			if (GZ > MC::MinZ) FlyTarget.Z = FMath::Max(FlyTarget.Z, GZ + 0.8);
		}
		TickFlyMovement(0.08f);
		return;
	}
	if (Id == TEXT("allay"))
	{
		// follow the player who gave the item, collect matching items
		AMCEntity* O = Owner.Get();
		const FMCItemStack& Want = GetItem(EMCEquipSlot::MainHand);
		if (!Want.IsEmpty() && Age % 10 == 0)
		{
			TArray<AMCEntity*> Near;
			World->GetEntitiesInBox(GetBox().Inflate(12.0), Near, this);
			for (AMCEntity* E : Near)
			{
				AMCItemEntity* IE = Cast<AMCItemEntity>(E);
				if (IE && IE->Stack.Id == Want.Id && !IE->bRemoved) { FlyTarget = IE->Pos + FVector(0, 0, 0.5); SubState = 1; break; }
			}
		}
		if (SubState == 1)
		{
			TArray<AMCEntity*> Close;
			World->GetEntitiesInBox(GetBox().Inflate(1.0), Close, this);
			for (AMCEntity* E : Close)
			{
				AMCItemEntity* IE = Cast<AMCItemEntity>(E);
				if (IE && IE->Stack.Id == Want.Id && !IE->bRemoved)
				{
					FMCItemStack S = IE->Stack;
					MobInventory.Insert(S);
					if (S.IsEmpty()) IE->Discard(); else IE->Stack = S;
					PlaySound(TEXT("item_pickup"), 0.3f, 1.4f);
					SubState = 2;
				}
			}
		}
		else if (SubState == 2 && O)
		{
			FlyTarget = O->GetEyePos();
			if (FVector::DistSquared(O->Pos, Pos) < 6.0)
			{
				for (FMCItemStack& S : MobInventory.Slots) if (!S.IsEmpty()) { World->SpawnItem(Pos, S, true, 1.f); S.Clear(); }
				SubState = 0;
			}
		}
		else if (O && FVector::DistSquared(O->Pos, Pos) > 9.0) FlyTarget = O->GetEyePos() + FVector(0, 0, 0.5);
		else if (FVector::DistSquared(FlyTarget, Pos) < 1.0 || Rand().NextInt(100) == 0) FlyTarget = Pos + FVector(Rand().FRange(-4.f, 4.f), Rand().FRange(-4.f, 4.f), Rand().FRange(-1.f, 2.f));
		TickFlyMovement(0.12f);
		return;
	}
	if (Id == TEXT("parrot"))
	{
		if (bSitting) { Vel.X = Vel.Y = 0.0; return; }
		AMCEntity* O = Owner.Get();
		if (bTamed && O && FVector::DistSquared(O->Pos, Pos) > 25.0) FlyTarget = O->GetEyePos() + FVector(Rand().FRange(-1.f, 1.f), Rand().FRange(-1.f, 1.f), 0.5);
		else if (FVector::DistSquared(FlyTarget, Pos) < 1.0 || Rand().NextInt(120) == 0)
		{
			FlyTarget = Pos + FVector(Rand().FRange(-8.f, 8.f), Rand().FRange(-8.f, 8.f), Rand().FRange(-2.f, 3.f));
		}
		// parrots mostly walk and only flutter when moving vertically
		if (bOnGround && Rand().NextInt(3) != 0 && FVector::DistSquared(FlyTarget, Pos) < 16.0) { TickWander(1.f, 60); return; }
		TickFlyMovement(0.1f);
		if (Vel.Z < 0.0 && !bOnGround) Vel.Z *= 0.6;
		return;
	}
	if (Id == TEXT("happy_ghast") || Id == TEXT("ghastling"))
	{
		AMCPlayer* P = Game ? Game->Player : nullptr;
		if (P && IsTemptedBy(P) && FVector::DistSquared(P->Pos, Pos) < 256.0) FlyTarget = P->GetEyePos() + FVector(0, 0, 2.0);
		else if (FVector::DistSquared(FlyTarget, Pos) < 4.0 || Rand().NextInt(200) == 0)
		{
			FlyTarget = Pos + FVector(Rand().FRange(-10.f, 10.f), Rand().FRange(-10.f, 10.f), Rand().FRange(-3.f, 3.f));
			// stay near the ground but not too low
			const int32 GZ = World->FindGroundZ(MC::FloorToInt(FlyTarget.X), MC::FloorToInt(FlyTarget.Y), MC::FloorToInt(FlyTarget.Z) + 8);
			if (GZ > MC::MinZ) FlyTarget.Z = FMath::Clamp(FlyTarget.Z, GZ + 2.0, GZ + 12.0);
		}
		TickFlyMovement(0.05f);
		return;
	}
	// generic flyer
	if (FVector::DistSquared(FlyTarget, Pos) < 2.0 || Rand().NextInt(100) == 0) FlyTarget = Pos + FVector(Rand().FRange(-6.f, 6.f), Rand().FRange(-6.f, 6.f), Rand().FRange(-2.f, 3.f));
	TickFlyMovement(0.1f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Swimmers: fish, squid, dolphins, guardians, axolotls, tadpoles, nautiluses

void AMCMob::TickSwimmerAI()
{
	if (!Def || !World) return;
	const FName Id = Def->Id;
	if (Id == TEXT("guardian") || Id == TEXT("elder_guardian") || Id == TEXT("zombie_nautilus") || (Id == TEXT("dolphin") && bAngry) || Id == TEXT("axolotl"))
	{
		if (Id == TEXT("axolotl"))
		{
			if (!Target.IsValid() && Age % 20 == 0)
				Target = FindNearestMob(8.f, [](AMCLiving* L) { const AMCMob* M = Cast<AMCMob>(L); return M && M->Def && (M->Def->Id == TEXT("drowned") || M->Def->Id == TEXT("guardian") || M->Def->Id == TEXT("cod") || M->Def->Id == TEXT("salmon") || M->Def->Id == TEXT("squid")); });
		}
		else TickCommonTargeting();
	}
	AMCLiving* T = Target.Get();
	if (T && !ValidTarget(T, this)) { Target.Reset(); T = nullptr; }
	// amphibians walk on land
	if (!bInWater && Def->bAmphibious) { if (T) { NavigateTo(T->Pos, 1.f); DoMeleeAttack(T); } else TickWander(1.f, 80); return; }
	if (Id == TEXT("guardian") || Id == TEXT("elder_guardian"))
	{
		if (T && CanSee(T) && FVector::DistSquared(T->Pos, Pos) < 225.0)
		{
			// laser: charge 80 ticks (40 for elder), then damage
			LookAt(T->GetEyePos(), 90.f, 90.f);
			Vel *= 0.5;
			++SpecialTimer;
			AimPos = T->GetEyePos();
			if (SpecialTimer == 1) PlaySound(TEXT("guardian_attack"), 1.f, 1.f);
			if (SpecialTimer >= (Id == TEXT("elder_guardian") ? 60 : 80))
			{
				FMCDamage D = FMCDamage::Mob(this); D.Type = TEXT("magic"); D.bMagic = true; D.bBypassArmor = true;
				T->Hurt(D, Game && Game->Difficulty == EMCDifficulty::Hard ? 2.f : 1.f);
				T->Hurt(FMCDamage::Mob(this), Def->AttackDamage);
				SpecialTimer = 0;
			}
			return;
		}
		SpecialTimer = 0;
		AimPos = FVector::ZeroVector;
		// elder guardians curse nearby players with mining fatigue
		if (Id == TEXT("elder_guardian") && Age % 1200 == 0 && Game && Game->Player && FVector::DistSquared(Game->Player->Pos, Pos) < 2500.0)
		{
			FMCEffectInstance E; E.Effect = EMCEffect::MiningFatigue; E.Duration = 6000; E.Amplifier = 2;
			Game->Player->AddEffect(E);
			PlaySound(TEXT("elder_guardian_curse"), 1.f, 1.f);
		}
	}
	if (T && (Id == TEXT("axolotl") || Id == TEXT("dolphin") || Id == TEXT("zombie_nautilus")))
	{
		FlyTarget = T->Pos + FVector(0, 0, T->Height * 0.5);
		TickSwimMovement(Id == TEXT("dolphin") ? 2.f : 1.2f);
		DoMeleeAttack(T);
		return;
	}
	if (PanicTime > 0 && Rand().NextInt(5) == 0 && Game && Game->Player) FlyTarget = Pos + (Pos - Game->Player->Pos).GetSafeNormal() * 6.0;
	// dolphins give players dolphin's grace
	if (Id == TEXT("dolphin") && Game && Game->Player && Game->Player->bInWater && FVector::DistSquared(Game->Player->Pos, Pos) < 64.0 && Age % 20 == 0)
	{
		FMCEffectInstance E; E.Effect = EMCEffect::DolphinsGrace; E.Duration = 100; Game->Player->AddEffect(E);
		FlyTarget = Game->Player->Pos;
	}
	// tadpoles grow into frogs
	if (Id == TEXT("tadpole") && Age > 24000 && Game)
	{
		if (AMCMob* F = Cast<AMCMob>(Game->SpawnMob(World, TEXT("frog"), Pos, false))) F->bPersistent = true;
		Discard();
		return;
	}
	const float Speed = Id == TEXT("dolphin") ? 2.2f : (Id == TEXT("squid") || Id == TEXT("glow_squid") ? 0.7f : 1.f);
	TickSwimMovement(Speed * (PanicTime > 0 ? 1.8f : 1.f));
}

// ---------------------------------------------------------------------------------------------------------------------
// Villagers & wandering traders

void AMCMob::GenerateTrades()
{
	if (!Def) return;
	FMCRandom R((uint64)GetTypeHash(Profession) ^ (uint64)VillagerLevel * 7919u ^ (uint64)EntityId);
	auto T = [&](const TCHAR* A, int32 AC, const TCHAR* B, int32 BC, const TCHAR* Out, int32 OC, int32 Max = 12, int32 XP = 2)
	{
		FMCTrade Tr;
		Tr.CostA = FMCItemStack::Of(FName(A), AC);
		if (B) Tr.CostB = FMCItemStack::Of(FName(B), BC);
		Tr.Result = FMCItemStack::Of(FName(Out), OC);
		Tr.MaxUses = Max; Tr.XP = XP;
		if (!Tr.CostA.IsEmpty() && !Tr.Result.IsEmpty()) Trades.Add(Tr);
	};
	if (Def->Id == TEXT("wandering_trader"))
	{
		if (Trades.Num() > 0) return;
		static const TCHAR* Goods[] = { TEXT("blue_ice"), TEXT("packed_ice"), TEXT("sea_pickle"), TEXT("slime_ball"), TEXT("glowstone"), TEXT("nautilus_shell"), TEXT("fern"), TEXT("sugar_cane"),
			TEXT("pumpkin"), TEXT("kelp"), TEXT("cactus"), TEXT("dandelion"), TEXT("poppy"), TEXT("oak_sapling"), TEXT("birch_sapling"), TEXT("cherry_sapling"), TEXT("brain_coral_block"),
			TEXT("red_mushroom"), TEXT("brown_mushroom"), TEXT("lily_pad"), TEXT("small_dripleaf"), TEXT("moss_block"), TEXT("pointed_dripstone"), TEXT("rooted_dirt"), TEXT("mud"), TEXT("sand") };
		for (int32 i = 0; i < 6; ++i)
		{
			const TCHAR* G = Goods[R.NextInt(UE_ARRAY_COUNT(Goods))];
			T(TEXT("emerald"), 1 + R.NextInt(3), nullptr, 0, G, 1 + R.NextInt(3), 5, 1);
		}
		T(TEXT("emerald"), 5, nullptr, 0, TEXT("tropical_fish_bucket"), 1, 4, 1);
		T(TEXT("emerald"), 3, nullptr, 0, TEXT("gunpowder"), 1, 8, 1);
		return;
	}
	static const TCHAR* Professions[] = { TEXT("farmer"), TEXT("librarian"), TEXT("cleric"), TEXT("armorer"), TEXT("weaponsmith"), TEXT("toolsmith"), TEXT("butcher"),
		TEXT("leatherworker"), TEXT("mason"), TEXT("fletcher"), TEXT("shepherd"), TEXT("cartographer"), TEXT("fisherman") };
	if (Profession.IsNone()) Profession = FName(Professions[R.NextInt(UE_ARRAY_COUNT(Professions))]);
	const FString P = Profession.ToString();
	const int32 L = FMath::Clamp(VillagerLevel, 1, 5);
	const int32 Before = Trades.Num();
	// only add the trades of levels not yet unlocked
	const int32 Have = Trades.Num();
	auto Level = [&](int32 Lv, TFunctionRef<void()> Fn) { if (L >= Lv && Have < Lv * 2) Fn(); };
	if (P == TEXT("farmer"))
	{
		Level(1, [&] { T(TEXT("wheat"), 20, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("bread"), 6, 16, 1); });
		Level(2, [&] { T(TEXT("pumpkin"), 6, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 1, nullptr, 0, TEXT("pumpkin_pie"), 4, 12, 5); });
		Level(3, [&] { T(TEXT("melon"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 3, nullptr, 0, TEXT("cookie"), 18, 12, 10); });
		Level(4, [&] { T(TEXT("emerald"), 1, nullptr, 0, TEXT("cake"), 1, 12, 15); T(TEXT("emerald"), 1, nullptr, 0, TEXT("suspicious_stew"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("emerald"), 3, nullptr, 0, TEXT("golden_carrot"), 3, 12, 30); T(TEXT("emerald"), 4, nullptr, 0, TEXT("glistering_melon_slice"), 3, 12, 30); });
	}
	else if (P == TEXT("librarian"))
	{
		Level(1, [&] { T(TEXT("paper"), 24, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 9, nullptr, 0, TEXT("bookshelf"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("book"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 1, nullptr, 0, TEXT("lantern"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("ink_sac"), 5, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 1, nullptr, 0, TEXT("glass"), 4, 12, 10); });
		Level(4, [&] { T(TEXT("writable_book"), 2, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 5, nullptr, 0, TEXT("clock"), 1, 12, 15); T(TEXT("emerald"), 4, nullptr, 0, TEXT("compass"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("emerald"), 20, nullptr, 0, TEXT("name_tag"), 1, 12, 30); });
		// enchanted books
		if (L >= 1 && Before == 0)
		{
			FMCTrade Tr;
			Tr.CostA = FMCItemStack::Of(TEXT("emerald"), 5 + R.NextInt(20));
			Tr.CostB = FMCItemStack::Of(TEXT("book"), 1);
			Tr.Result = FMCItemStack::Of(TEXT("enchanted_book"), 1);
			FMCEnchantLevel E;
			static const EMCEnchant Pool[] = { EMCEnchant::Protection, EMCEnchant::Sharpness, EMCEnchant::Efficiency, EMCEnchant::Unbreaking, EMCEnchant::Mending, EMCEnchant::Fortune, EMCEnchant::Power, EMCEnchant::FeatherFalling, EMCEnchant::Looting, EMCEnchant::SilkTouch };
			E.Id = Pool[R.NextInt(UE_ARRAY_COUNT(Pool))];
			E.Level = (uint8)(1 + R.NextInt(MCEnchants::Info(E.Id).MaxLevel));
			Tr.Result.MutableExtra().StoredEnchants.Add(E);
			Tr.MaxUses = 12; Tr.XP = 1;
			Trades.Add(Tr);
		}
	}
	else if (P == TEXT("cleric"))
	{
		Level(1, [&] { T(TEXT("rotten_flesh"), 32, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("redstone"), 2, 12, 1); });
		Level(2, [&] { T(TEXT("gold_ingot"), 3, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 1, nullptr, 0, TEXT("lapis_lazuli"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("rabbit_foot"), 2, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 4, nullptr, 0, TEXT("glowstone"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("turtle_scute"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("glass_bottle"), 9, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 5, nullptr, 0, TEXT("ender_pearl"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("nether_wart"), 22, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 3, nullptr, 0, TEXT("experience_bottle"), 1, 12, 30); });
	}
	else if (P == TEXT("armorer"))
	{
		Level(1, [&] { T(TEXT("coal"), 15, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 5, nullptr, 0, TEXT("iron_helmet"), 1, 12, 1); T(TEXT("emerald"), 9, nullptr, 0, TEXT("iron_chestplate"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("iron_ingot"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 36, nullptr, 0, TEXT("bell"), 1, 12, 5); T(TEXT("emerald"), 7, nullptr, 0, TEXT("iron_leggings"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("lava_bucket"), 1, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 5, nullptr, 0, TEXT("shield"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("emerald"), 18, nullptr, 0, TEXT("diamond_leggings"), 1, 3, 15); T(TEXT("emerald"), 13, nullptr, 0, TEXT("diamond_boots"), 1, 3, 15); });
		Level(5, [&] { T(TEXT("emerald"), 21, nullptr, 0, TEXT("diamond_chestplate"), 1, 3, 30); T(TEXT("emerald"), 13, nullptr, 0, TEXT("diamond_helmet"), 1, 3, 30); });
	}
	else if (P == TEXT("weaponsmith"))
	{
		Level(1, [&] { T(TEXT("coal"), 15, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 3, nullptr, 0, TEXT("iron_axe"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("iron_ingot"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 5, nullptr, 0, TEXT("iron_sword"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("flint"), 24, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 8, nullptr, 0, TEXT("iron_spear"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("diamond"), 1, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 17, nullptr, 0, TEXT("diamond_axe"), 1, 3, 15); });
		Level(5, [&] { T(TEXT("emerald"), 13, nullptr, 0, TEXT("diamond_sword"), 1, 3, 30); });
	}
	else if (P == TEXT("toolsmith"))
	{
		Level(1, [&] { T(TEXT("coal"), 15, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("stone_pickaxe"), 1, 12, 1); T(TEXT("emerald"), 1, nullptr, 0, TEXT("stone_shovel"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("iron_ingot"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 36, nullptr, 0, TEXT("bell"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("flint"), 30, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 6, nullptr, 0, TEXT("iron_pickaxe"), 1, 3, 10); });
		Level(4, [&] { T(TEXT("diamond"), 1, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 12, nullptr, 0, TEXT("diamond_axe"), 1, 3, 15); });
		Level(5, [&] { T(TEXT("emerald"), 13, nullptr, 0, TEXT("diamond_pickaxe"), 1, 3, 30); });
	}
	else if (P == TEXT("butcher"))
	{
		Level(1, [&] { T(TEXT("chicken"), 14, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("porkchop"), 7, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("rabbit_stew"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("coal"), 15, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("cooked_porkchop"), 5, 16, 5); });
		Level(3, [&] { T(TEXT("mutton"), 7, nullptr, 0, TEXT("emerald"), 1, 16, 20); T(TEXT("beef"), 10, nullptr, 0, TEXT("emerald"), 1, 16, 20); });
		Level(4, [&] { T(TEXT("dried_kelp_block"), 10, nullptr, 0, TEXT("emerald"), 1, 12, 30); });
		Level(5, [&] { T(TEXT("sweet_berries"), 10, nullptr, 0, TEXT("emerald"), 1, 12, 30); });
	}
	else if (P == TEXT("leatherworker"))
	{
		Level(1, [&] { T(TEXT("leather"), 6, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 3, nullptr, 0, TEXT("leather_leggings"), 1, 12, 1); T(TEXT("emerald"), 7, nullptr, 0, TEXT("leather_chestplate"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("flint"), 26, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 5, nullptr, 0, TEXT("leather_helmet"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("rabbit_hide"), 9, nullptr, 0, TEXT("emerald"), 1, 12, 20); T(TEXT("emerald"), 7, nullptr, 0, TEXT("leather_chestplate"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("turtle_scute"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 6, nullptr, 0, TEXT("leather_horse_armor"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("emerald"), 6, nullptr, 0, TEXT("saddle"), 1, 12, 30); });
	}
	else if (P == TEXT("mason"))
	{
		Level(1, [&] { T(TEXT("clay_ball"), 10, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("bricks"), 10, 16, 1); });
		Level(2, [&] { T(TEXT("stone"), 20, nullptr, 0, TEXT("emerald"), 1, 16, 10); T(TEXT("emerald"), 1, nullptr, 0, TEXT("chiseled_stone_bricks"), 4, 16, 5); });
		Level(3, [&] { T(TEXT("granite"), 16, nullptr, 0, TEXT("emerald"), 1, 16, 20); T(TEXT("emerald"), 1, nullptr, 0, TEXT("polished_andesite"), 4, 16, 10); });
		Level(4, [&] { T(TEXT("quartz"), 12, nullptr, 0, TEXT("emerald"), 1, 12, 30); T(TEXT("emerald"), 1, nullptr, 0, TEXT("white_terracotta"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("emerald"), 1, nullptr, 0, TEXT("quartz_pillar"), 1, 12, 30); T(TEXT("emerald"), 1, nullptr, 0, TEXT("quartz_block"), 1, 12, 30); });
	}
	else if (P == TEXT("fletcher"))
	{
		Level(1, [&] { T(TEXT("stick"), 32, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, nullptr, 0, TEXT("arrow"), 16, 12, 1); T(TEXT("emerald"), 1, TEXT("gravel"), 10, TEXT("flint"), 10, 12, 1); });
		Level(2, [&] { T(TEXT("flint"), 26, nullptr, 0, TEXT("emerald"), 1, 12, 10); T(TEXT("emerald"), 2, nullptr, 0, TEXT("bow"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("string"), 14, nullptr, 0, TEXT("emerald"), 1, 16, 20); T(TEXT("emerald"), 3, nullptr, 0, TEXT("crossbow"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("feather"), 24, nullptr, 0, TEXT("emerald"), 1, 16, 30); });
		Level(5, [&] { T(TEXT("tripwire_hook"), 8, nullptr, 0, TEXT("emerald"), 1, 12, 30); });
	}
	else if (P == TEXT("shepherd"))
	{
		Level(1, [&] { T(TEXT("white_wool"), 18, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 2, nullptr, 0, TEXT("shears"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("black_dye"), 12, nullptr, 0, TEXT("emerald"), 1, 16, 10); T(TEXT("emerald"), 1, nullptr, 0, TEXT("red_wool"), 1, 16, 5); });
		Level(3, [&] { T(TEXT("yellow_dye"), 12, nullptr, 0, TEXT("emerald"), 1, 16, 20); T(TEXT("emerald"), 3, nullptr, 0, TEXT("white_bed"), 1, 12, 10); });
		Level(4, [&] { T(TEXT("brown_dye"), 12, nullptr, 0, TEXT("emerald"), 1, 16, 30); });
		Level(5, [&] { T(TEXT("emerald"), 2, nullptr, 0, TEXT("painting"), 3, 12, 30); });
	}
	else if (P == TEXT("cartographer"))
	{
		Level(1, [&] { T(TEXT("paper"), 24, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 7, nullptr, 0, TEXT("map"), 1, 12, 1); });
		Level(2, [&] { T(TEXT("glass_pane"), 11, nullptr, 0, TEXT("emerald"), 1, 16, 10); T(TEXT("emerald"), 13, TEXT("compass"), 1, TEXT("filled_map"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("compass"), 1, nullptr, 0, TEXT("emerald"), 1, 12, 20); });
		Level(4, [&] { T(TEXT("emerald"), 7, nullptr, 0, TEXT("item_frame"), 1, 12, 15); });
		Level(5, [&] { T(TEXT("emerald"), 8, nullptr, 0, TEXT("globe_banner_pattern"), 1, 12, 30); });
	}
	else if (P == TEXT("fisherman"))
	{
		Level(1, [&] { T(TEXT("string"), 20, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("coal"), 10, nullptr, 0, TEXT("emerald"), 1, 16, 2); T(TEXT("emerald"), 1, TEXT("cod"), 6, TEXT("cooked_cod"), 6, 16, 1); });
		Level(2, [&] { T(TEXT("cod"), 15, nullptr, 0, TEXT("emerald"), 1, 16, 10); T(TEXT("emerald"), 2, nullptr, 0, TEXT("campfire"), 1, 12, 5); });
		Level(3, [&] { T(TEXT("salmon"), 13, nullptr, 0, TEXT("emerald"), 1, 16, 20); T(TEXT("emerald"), 3, nullptr, 0, TEXT("fishing_rod"), 1, 3, 10); });
		Level(4, [&] { T(TEXT("tropical_fish"), 6, nullptr, 0, TEXT("emerald"), 1, 12, 30); });
		Level(5, [&] { T(TEXT("pufferfish"), 4, nullptr, 0, TEXT("emerald"), 1, 12, 30); });
	}
}

void AMCMob::TickVillagerAI()
{
	if (!Def || !World) return;
	const bool bTrader = Def->Id == TEXT("wandering_trader");
	if (TradingPlayer.IsValid()) { StopNavigation(); MoveForward = 0.f; LookAt(TradingPlayer->GetEyePos(), 20.f, 20.f); return; }
	if (!bHasHome) { HomePos = BlockPos(); bHasHome = true; }
	// flee from zombies and illagers
	if (Age % 10 == 0)
	{
		AMCLiving* Threat = FindNearestMob(8.f, [](AMCLiving* L)
		{
			const AMCMob* M = Cast<AMCMob>(L);
			return M && M->Def && (M->Def->AI == EMCMobAI::Zombie || M->Def->Id == TEXT("vindicator") || M->Def->Id == TEXT("pillager") || M->Def->Id == TEXT("evoker") || M->Def->Id == TEXT("ravager") || M->Def->Id == TEXT("vex")) && M->Def->Id != TEXT("piglin");
		});
		if (Threat) { WanderTarget = Pos + (Pos - Threat->Pos).GetSafeNormal() * 10.0; bHasWander = true; PanicTime = 60; StopNavigation(); }
	}
	if (PanicTime > 0 && bHasWander) { NavigateTo(WanderTarget, 1.3f); return; }
	// wandering traders despawn after a while
	if (bTrader && Age > 48000 && !bPersistent) { Discard(); return; }
	// night: stay close to home (beds are placed in houses)
	if (Game && !Game->IsDay() && bHasHome && FVector::DistSquared(FVector(HomePos.X + 0.5, HomePos.Y + 0.5, HomePos.Z), Pos) > 16.0 && !bTrader)
	{
		NavigateTo(FVector(HomePos.X + 0.5, HomePos.Y + 0.5, HomePos.Z), 0.7f);
		return;
	}
	// look at players
	if (Game && Game->Player && FVector::DistSquared(Game->Player->Pos, Pos) < 36.0 && Rand().NextInt(20) == 0) { LookAt(Game->Player->GetEyePos(), 30.f, 30.f); MoveForward = 0.f; return; }
	// gossip/leveling particles occasionally
	TickWander(0.5f, IsBaby() ? 40 : 100);
}

// ---------------------------------------------------------------------------------------------------------------------
// Golems

void AMCMob::TickGolemAI()
{
	if (!Def || !World) return;
	const FName Id = Def->Id;
	AMCLiving* T = Target.Get();
	if (T && !ValidTarget(T, this)) { Target.Reset(); T = nullptr; }
	if (!T && Age % 10 == 0 && Id != TEXT("copper_golem"))
	{
		T = FindNearestMob(Id == TEXT("snow_golem") ? 10.f : 16.f, [](AMCLiving* L) { return IsHostileMob(L); });
		Target = T;
	}
	if (Id == TEXT("snow_golem"))
	{
		// leaves a snow trail in cold biomes and throws snowballs
		const FMCBlockPos P = BlockPos();
		if (Game && Game->Rules.bMobGriefing && World->GetState(P) == 0 && FMCBlocks::IsSolid(World->GetState(P.Down())) && FMCBlocks::IsOpaque(World->GetState(P.Down())))
			World->SetState(P, FMCBlocks::C.Snow, MCSet_Default);
		if (T)
		{
			LookAt(T->GetEyePos(), 30.f, 30.f);
			if (FVector::DistSquared(T->Pos, Pos) > 100.0) NavigateTo(T->Pos, 1.f);
			else { StopNavigation(); MoveForward = 0.f; }
			if (--ShootTime <= 0 && CanSee(T)) { ShootProjectile(TEXT("snowball"), T, 1.6f, 1.f); PlaySound(TEXT("snow_golem_shoot"), 1.f, 1.f); ShootTime = 20; }
			return;
		}
		TickWander(1.f, 120);
		return;
	}
	if (Id == TEXT("copper_golem"))
	{
		// copper golems wander between copper chests and press copper buttons now and then
		if (Rand().NextInt(400) == 0)
		{
			for (int32 dx = -3; dx <= 3; ++dx) for (int32 dy = -3; dy <= 3; ++dy) for (int32 dz = -1; dz <= 2; ++dz)
			{
				const FMCBlockPos B = BlockPos() + FIntVector(dx, dy, dz);
				const FMCBlock& Blk = FMCBlocks::GetByState(World->GetState(B));
				if (Blk.Name.ToString().Contains(TEXT("copper")) && Blk.Model == EMCModel::Button) { Blk.Behavior->OnUse(*World, B, World->GetState(B), nullptr, EMCFace::Up, FVector(0.5)); dx = 4; dy = 4; break; }
			}
		}
		TickWander(0.8f, 80);
		return;
	}
	// iron golem
	if (T)
	{
		LookAt(T->GetEyePos(), 30.f, 30.f);
		NavigateTo(T->Pos, 1.f);
		DoMeleeAttack(T);
		return;
	}
	// offer poppies to villager children occasionally
	TickWander(0.6f, 240);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tameable: wolves and cats

void AMCMob::TickTameableAI()
{
	if (!Def || !World) return;
	AMCLiving* OwnerL = Cast<AMCLiving>(Owner.Get());
	AMCLiving* T = Target.Get();
	if (T && !ValidTarget(T, this)) { Target.Reset(); T = nullptr; bAngry = false; }
	if (bSitting) { StopNavigation(); MoveForward = 0.f; if (T && bTamed) {} else return; }
	if (bTamed && OwnerL)
	{
		// defend the owner / attack what the owner attacks
		if (!T && Def->Id == TEXT("wolf"))
		{
			if (OwnerL->LastAttacker.IsValid() && OwnerL->Age - OwnerL->LastAttackerTime < 100) T = Cast<AMCLiving>(OwnerL->LastAttacker.Get());
			else if (OwnerL->LastHurtMob.IsValid()) T = Cast<AMCLiving>(OwnerL->LastHurtMob.Get());
			if (T && T == this) T = nullptr;
			if (T && ValidTarget(T, this) && !(Cast<AMCMob>(T) && Cast<AMCMob>(T)->bTamed)) Target = T; else T = nullptr;
		}
		if (T && !bSitting)
		{
			LookAt(T->GetEyePos(), 30.f, 30.f);
			NavigateTo(T->Pos, 1.2f);
			DoMeleeAttack(T);
			return;
		}
		if (bSitting) return;
		const double D = FVector::DistSquared(OwnerL->Pos, Pos);
		if (D > 144.0 && OwnerL->bOnGround)
		{
			// teleport to the owner
			for (int32 Try = 0; Try < 10; ++Try)
			{
				const FVector C = OwnerL->Pos + FVector(Rand().Range(-3, 3), Rand().Range(-3, 3), 0);
				const FMCBlockPos B(MC::FloorToInt(C.X), MC::FloorToInt(C.Y), MC::FloorToInt(C.Z));
				if (FMCPathfinder::IsWalkable(*World, B, 1, false)) { TeleportTo(FVector(B.X + 0.5, B.Y + 0.5, B.Z)); StopNavigation(); break; }
			}
			return;
		}
		if (D > 16.0) { NavigateTo(OwnerL->Pos, 1.f); return; }
		StopNavigation();
		MoveForward = 0.f;
		if (Rand().NextInt(40) == 0) LookAt(OwnerL->GetEyePos(), 20.f, 20.f);
		return;
	}
	// wild
	if (T && bAngry)
	{
		LookAt(T->GetEyePos(), 30.f, 30.f);
		NavigateTo(T->Pos, 1.2f);
		DoMeleeAttack(T);
		return;
	}
	// wolves hunt sheep, rabbits, foxes and skeletons; cats hunt rabbits
	if (Age % 40 == 0 && Rand().NextInt(4) == 0)
	{
		const bool bWolf = Def->Id == TEXT("wolf");
		Target = FindNearestMob(12.f, [bWolf](AMCLiving* L)
		{
			const AMCMob* M = Cast<AMCMob>(L);
			if (!M || !M->Def) return false;
			const FName I = M->Def->Id;
			return bWolf ? (I == TEXT("sheep") || I == TEXT("rabbit") || I == TEXT("fox") || I == TEXT("skeleton") || I == TEXT("stray")) : (I == TEXT("rabbit") || I == TEXT("chicken"));
		});
		if (Target.IsValid()) { bAngry = true; AngerTime = 200; }
	}
	TickAnimalAI();
}

// ---------------------------------------------------------------------------------------------------------------------
// Illagers: vindicators, evokers, witches, ravagers

void AMCMob::TickIllagerAI()
{
	if (!Def || !World) return;
	const FName Id = Def->Id;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	if (!T) { TickWander(0.6f, 120); return; }
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const double D = FVector::DistSquared(T->Pos, Pos);
	if (Id == TEXT("witch"))
	{
		// drink potions when hurt / burning / underwater, throw splash potions otherwise
		if (SubState > 0)
		{
			if (--SubState == 0)
			{
				FMCEffectInstance E;
				E.Effect = IsOnFire() ? EMCEffect::FireResistance : (bEyesInWater ? EMCEffect::WaterBreathing : EMCEffect::InstantHealth);
				E.Duration = 3600;
				AddEffect(E);
				PlaySound(TEXT("witch_drink"), 1.f, 1.f);
			}
			MoveForward = 0.f;
			return;
		}
		if ((Health < MaxHealth * 0.5f || IsOnFire() || bEyesInWater) && Rand().NextInt(10) == 0) { SubState = 32; return; }
		if (D > 64.0 || !CanSee(T)) NavigateTo(T->Pos, 1.f);
		else { StopNavigation(); MoveForward = 0.f; }
		if (--ShootTime <= 0 && D < 100.0 && CanSee(T)) { ShootProjectile(TEXT("potion"), T, 0.75f, 8.f); PlaySound(TEXT("witch_throw"), 1.f, 1.f); ShootTime = 60; }
		return;
	}
	if (Id == TEXT("evoker"))
	{
		// fangs when close, vexes every so often
		if (--SpecialTimer <= 0 && D < 400.0)
		{
			if (Rand().NextInt(3) == 0 && Game)
			{
				for (int32 i = 0; i < 3; ++i) if (AMCMob* V = Cast<AMCMob>(Game->SpawnMob(World, TEXT("vex"), Pos + FVector(Rand().FRange(-1.f, 1.f), Rand().FRange(-1.f, 1.f), 1.0), false))) { V->Target = T; V->Owner = this; }
				PlaySound(TEXT("evoker_prepare_summon"), 1.f, 1.f);
				SpecialTimer = 340;
			}
			else
			{
				// a line of fangs towards the target
				const FVector Dir = (T->Pos - Pos).GetSafeNormal2D();
				for (int32 i = 1; i <= 16; ++i)
				{
					const FVector F = Pos + Dir * (1.25 * i);
					World->SpawnParticles(TEXT("evoker_fangs"), F, 3, 0.2f, FVector(0, 0, 0.3), FColor(200, 200, 180));
					TArray<AMCEntity*> Hit;
					World->GetEntitiesInBox(FMCBox(F - FVector(0.5, 0.5, 0), F + FVector(0.5, 0.5, 1)), Hit, this);
					for (AMCEntity* E : Hit) if (E->IsLiving() && E != this) { FMCDamage Dm = FMCDamage::Of(TEXT("magic")); Dm.Attacker = this; E->Hurt(Dm, 6.f); }
				}
				PlaySound(TEXT("evoker_fangs_attack"), 1.f, 1.f);
				SpecialTimer = 100;
			}
		}
		// keep some distance
		if (D < 36.0) { MoveForward = -0.6f; } else if (D > 144.0) NavigateTo(T->Pos, 0.8f); else MoveForward = 0.f;
		return;
	}
	if (Id == TEXT("ravager"))
	{
		// roar when blocked, trample leaves / crops
		if (bHorizontalCollision && Game && Game->Rules.bMobGriefing)
		{
			for (int32 dz = 0; dz <= 2; ++dz)
			{
				const FMCBlockPos Front = BlockPos() + FIntVector(FMath::RoundToInt(FMath::Cos(FMath::DegreesToRadians(Yaw))), FMath::RoundToInt(FMath::Sin(FMath::DegreesToRadians(Yaw))), dz);
				const FMCBlock& B = FMCBlocks::GetByState(World->GetState(Front));
				if (B.Has(MCB_Leaves) || B.Has(MCB_Plant)) World->DestroyBlock(Front, true, this);
			}
		}
		if (HurtTime == 9 && Rand().NextInt(3) == 0)
		{
			TArray<AMCEntity*> Around;
			World->GetEntitiesInBox(GetBox().Inflate(4.0), Around, this);
			for (AMCEntity* E : Around) if (AMCLiving* L = Cast<AMCLiving>(E)) { L->Hurt(FMCDamage::Mob(this), 6.f); L->Knockback(1.0, Pos.X - L->Pos.X, Pos.Y - L->Pos.Y); }
			PlaySound(TEXT("ravager_roar"), 1.f, 1.f);
		}
	}
	NavigateTo(T->Pos, Id == TEXT("vindicator") ? 1.1f : 1.f);
	DoMeleeAttack(T);
}

// ---------------------------------------------------------------------------------------------------------------------
// Shulker: stationary, peeks and fires homing bullets

void AMCMob::TickShulkerAI()
{
	if (!World) return;
	Vel.X = Vel.Y = 0.0;
	MoveForward = 0.f;
	TickCommonTargeting();
	AMCLiving* T = Target.Get();
	const int32 OpenTarget = T ? 100 : (Rand().NextInt(200) == 0 ? 30 : 0);
	if (SubState < OpenTarget) SubState = FMath::Min(OpenTarget, SubState + 5);
	else if (SubState > OpenTarget && Rand().NextInt(20) == 0) SubState = FMath::Max(OpenTarget, SubState - 5);
	BaseArmor = SubState > 10 ? 0.f : 20.f;
	if (T && CanSee(T))
	{
		LookAt(T->GetEyePos(), 20.f, 20.f);
		if (--ShootTime <= 0)
		{
			ShootProjectile(TEXT("shulker_bullet"), T, 0.4f, 0.f);
			PlaySound(TEXT("shulker_shoot"), 2.f, 1.f);
			ShootTime = 20 + Rand().NextInt(90);
		}
	}
	// shulkers attach to blocks: if the block below vanished, teleport
	if (!FMCBlocks::IsSolid(World->GetState(BlockBelow())) && Rand().NextInt(20) == 0) TeleportRandomly(8.f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Warden: blind, tracks vibrations, sonic boom

void AMCMob::TickWardenAI()
{
	if (!World || !Def) return;
	AMCPlayer* P = Game ? Game->Player : nullptr;
	// darkness pulse
	if (Age % 120 == 0 && P && FVector::DistSquared(P->Pos, Pos) < 400.0 && !P->IsCreative())
	{
		FMCEffectInstance E; E.Effect = EMCEffect::Darkness; E.Duration = 260; E.bAmbient = true;
		P->AddEffect(E);
	}
	// vibrations: moving (not sneaking) players nearby raise anger
	if (P && P->World == World && !P->IsCreative() && !P->IsSpectator())
	{
		const double D = FVector::DistSquared(P->Pos, Pos);
		const bool bLoud = !P->bSneaking && (FVector::DistSquared(P->Pos, P->PrevPos) > 0.0004 || P->bMining || P->bUsingItem);
		if (D < 256.0 && bLoud) Anger = FMath::Min(150, Anger + (D < 25.0 ? 10 : 3));
		if (D < 9.0) Anger = FMath::Min(150, Anger + 5); // sniffing
	}
	if (Anger > 0 && Age % 20 == 0) Anger = FMath::Max(0, Anger - 1);
	if (Anger >= 80 && P) { Target = P; bAngry = true; }
	else if (Anger < 40) { Target.Reset(); bAngry = false; }
	AMCLiving* T = Target.Get();
	if (T && !ValidTarget(T, this)) { Target.Reset(); T = nullptr; }
	if (SpecialTimer > 0) --SpecialTimer;
	if (!T)
	{
		// dig back into the ground when calm for long
		if (Anger == 0 && Age > 1200 && Rand().NextInt(1200) == 0 && !bPersistent) { PlaySound(TEXT("warden_dig"), 1.f, 1.f); Discard(); return; }
		TickWander(0.5f, 160);
		return;
	}
	LookAt(T->GetEyePos(), 30.f, 30.f);
	const double D = FVector::DistSquared(T->Pos, Pos);
	if (D > 49.0 && D < 225.0 && SpecialTimer <= 0 && CanSee(T))
	{
		// sonic boom: ignores armour and shields
		SpecialTimer = 60;
		const FVector Dir = (T->GetEyePos() - GetEyePos()).GetSafeNormal();
		for (int32 i = 1; i < 15; ++i) World->SpawnParticles(TEXT("sonic_boom"), GetEyePos() + Dir * i, 1, 0.f);
		PlaySound(TEXT("warden_sonic_boom"), 3.f, 1.f);
		FMCDamage Dm = FMCDamage::Of(TEXT("sonic_boom"));
		Dm.Attacker = this; Dm.bBypassArmor = true; Dm.bNoKnockback = false;
		T->Hurt(Dm, 10.f);
		T->Vel += Dir * 2.5 + FVector(0, 0, 0.5);
		return;
	}
	NavigateTo(T->Pos, 1.2f);
	DoMeleeAttack(T);
}

// ---------------------------------------------------------------------------------------------------------------------
// Special per-mob behaviour that runs alongside the archetype

void AMCMob::TickSpecialAbilities()
{
	if (!Def || !World) return;
	const FName Id = Def->Id;
	// zombies convert in water (drowned) or in the cold? husks -> zombies underwater
	if ((Id == TEXT("zombie") || Id == TEXT("husk")) && bEyesInWater && Game)
	{
		if (++SubState > 600)
		{
			if (AMCMob* M = Cast<AMCMob>(Game->SpawnMob(World, Id == TEXT("husk") ? TEXT("zombie") : TEXT("drowned"), Pos, false))) { M->Yaw = Yaw; M->bPersistent = bPersistent; }
			Discard();
			return;
		}
	}
	// piglins pick up gold to barter
	if (Id == TEXT("piglin") && Anger == 0 && Age % 10 == 0)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(GetBox().Inflate(1.0), Near, this);
		for (AMCEntity* E : Near)
		{
			AMCItemEntity* IE = Cast<AMCItemEntity>(E);
			if (IE && !IE->bRemoved && IE->Stack.Item().Name == TEXT("gold_ingot"))
			{
				IE->Stack.Count -= 1;
				if (IE->Stack.Count <= 0) IE->Discard();
				GetItem(EMCEquipSlot::OffHand) = FMCItemStack::Of(TEXT("gold_ingot"), 1);
				Anger = 120; // admire for 6 seconds
				PlaySound(TEXT("piglin_admiring_item"), 1.f, 1.f);
				break;
			}
		}
	}
	// piglins & hoglins zombify in the overworld
	if ((Id == TEXT("piglin") || Id == TEXT("piglin_brute") || Id == TEXT("hoglin")) && World->Dim != EMCDimension::Nether && Game)
	{
		if (++SpecialTimer > 300)
		{
			if (AMCMob* M = Cast<AMCMob>(Game->SpawnMob(World, Id == TEXT("hoglin") ? TEXT("zoglin") : TEXT("zombified_piglin"), Pos, false))) { M->Yaw = Yaw; M->bPersistent = bPersistent; }
			World->SpawnParticles(TEXT("smoke"), Pos + FVector(0, 0, Height * 0.5), 12, 0.5f);
			Discard();
			return;
		}
	}
	// striders shiver outside lava
	if (Id == TEXT("strider"))
	{
		const bool bOnLava = FMCBlocks::Info(World->GetState(BlockPos())).Block == FMCBlocks::C.LavaId || FMCBlocks::Info(World->GetState(BlockBelow())).Block == FMCBlocks::C.LavaId;
		if (bOnLava) { Vel.Z = FMath::Max(Vel.Z, 0.0); bOnGround = true; }
	}
	// foxes sleep during the day
	if (Id == TEXT("fox") && Game) bSitting = Game->IsDay() && !Target.IsValid() && PanicTime == 0 && Rand().NextInt(2) == 0 ? bSitting : false;
}
