// Simple entities: dropped items, XP orbs, falling blocks, primed TNT, end crystals, lightning, item frames,
// eyes of ender, fireworks, lingering clouds.
#include "Game/MCEntities.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCGame.h"
#include "Render/MCRig.h"
#include "Render/MCAssets.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// =====================================================================================================================
// Dropped item

AMCItemEntity::AMCItemEntity()
{
	Kind = EMCEntityKind::Item;
	TypeId = TEXT("item");
	Width = Height = 0.25f;
	EyeHeight = 0.125f;
	Gravity = 0.04;
	StepHeight = 0.f;
	ItemVisual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("ItemVisual"));
	ItemVisual->SetupAttachment(VisualRoot);
}

void AMCItemEntity::InitEntity()
{
	Bob = Rand().NextFloat() * PI * 2.f;
	Yaw = Rand().NextFloat() * 360.f;
}

void AMCItemEntity::SetStack(const FMCItemStack& S)
{
	Stack = S.Copy();
	bFireImmune = !Stack.IsEmpty() && (Stack.Item().bFireResistant || Stack.Item().Name.ToString().StartsWith(TEXT("netherite")) || Stack.Item().Name == TEXT("nether_star"));
	ItemVisual->SetStack(Stack, 0);
	ItemVisual->SetRelativeScale3D(FVector(ItemVisual->IsBlockModel() ? 0.25f : 0.5f));
}

void AMCItemEntity::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	if (Stack.IsEmpty()) { Discard(); return; }
	if (PickupDelay > 0 && PickupDelay != 32767) --PickupDelay;

	if (bInWater)
	{
		// float up to the surface
		Vel *= 0.99;
		if (Vel.Z < 0.06) Vel.Z += 5.0e-4 * 20.0 * 0.05 + 0.0005;
	}
	else if (bInLava && bFireImmune)
	{
		Vel.Z = FMath::Min(Vel.Z + 0.01, 0.06);
	}
	else Vel.Z -= Gravity;

	if (!bOnGround || Vel.SizeSquared() > 1e-5 || Age % 4 == 0) Move(Vel);
	double F = 0.98;
	if (bOnGround)
	{
		const FMCState Below = World->GetState(FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z - 0.999)));
		F = FMCBlocks::GetByState(Below).Friction * 0.98;
	}
	Vel.X *= F; Vel.Y *= F; Vel.Z *= 0.98;
	if (bOnGround && Vel.Z < 0.0) Vel.Z *= -0.5;

	if (bInLava && !bFireImmune)
	{
		PlaySound(TEXT("fire_extinguish"), 0.4f, 2.f + Rand().NextFloat() * 0.4f);
		World->SpawnParticles(TEXT("smoke_large"), Pos, 4, 0.1f);
		Discard();
		return;
	}
	if (Age % 40 == 0 || (Age < 40 && Age % 5 == 0)) TryMerge();
	if (Age >= Lifetime && PickupDelay != 32767) Discard();
}

void AMCItemEntity::TryMerge()
{
	if (!World || Stack.Count >= Stack.MaxStack()) return;
	TArray<AMCEntity*> Near;
	World->GetEntitiesInBox(GetBox().Inflate(0.5).Expand(FVector(0, 0, 0.25)), Near, this);
	for (AMCEntity* E : Near)
	{
		AMCItemEntity* O = Cast<AMCItemEntity>(E);
		if (!O || O->bRemoved || O->Stack.IsEmpty() || !O->Stack.CanStackWith(Stack) || O->Stack.Id != Stack.Id) continue;
		const int32 Space = Stack.MaxStack() - Stack.Count;
		const int32 N = FMath::Min(Space, O->Stack.Count);
		if (N <= 0) break;
		Stack.Count += N;
		O->Stack.Count -= N;
		PickupDelay = FMath::Max(PickupDelay, O->PickupDelay);
		Age = FMath::Min(Age, O->Age);
		if (O->Stack.Count <= 0) O->Discard();
	}
}

void AMCItemEntity::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	const float T = (Age + Alpha) / 20.f;
	const float BobZ = (FMath::Sin(T * 2.f + Bob) * 0.1f + 0.1f + 0.15f);
	VisualRoot->SetRelativeLocation(FVector(0, 0, BobZ * MC::BlockSizeF));
	VisualRoot->SetRelativeRotation(FRotator(0.f, FMath::Fmod(T * 57.2958f * 1.0f + Yaw, 360.f), 0.f));
}

void AMCItemEntity::OnPlayerTouch(AMCPlayer* Player)
{
	if (!Player || PickupDelay > 0 || bRemoved || Player->Health <= 0.f || Player->IsSpectator()) return;
	if (Thrower.Get() == Player && Age < 40) return;
	const int32 Before = Stack.Count;
	FMCItemStack Copy = Stack.Copy();
	Player->AddItem(Copy);
	const int32 Taken = Before - (Copy.IsEmpty() ? 0 : Copy.Count);
	if (Taken <= 0) return;
	Player->PlaySound(TEXT("item_pickup"), 0.2f, (Rand().NextFloat() - Rand().NextFloat()) * 1.4f + 2.f);
	if (Copy.IsEmpty()) Discard();
	else Stack = Copy;
}

bool AMCItemEntity::Hurt(const FMCDamage& D, float Amount)
{
	if (bFireImmune && (D.bFire)) return false;
	if (D.bExplosion && !Stack.IsEmpty() && Stack.Item().Name == TEXT("nether_star")) return false;
	if (D.bFire || D.bExplosion || D.Type == TEXT("cactus") || D.Type == TEXT("out_of_world")) { Discard(); return true; }
	return false;
}

void AMCItemEntity::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Stack << PickupDelay << Lifetime;
	if (Ar.IsLoading()) SetStack(Stack);
}

// =====================================================================================================================
// Experience orb

AMCXPOrb::AMCXPOrb()
{
	Kind = EMCEntityKind::XPOrb;
	TypeId = TEXT("experience_orb");
	Width = Height = 0.5f;
	Gravity = 0.03;
	StepHeight = 0.f;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(VisualRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetRelativeLocation(FVector(0, 0, 12));
}

int32 AMCXPOrb::SplitValue(int32 Total)
{
	static const int32 Sizes[] = { 2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3, 1 };
	for (int32 S : Sizes) if (Total >= S) return S;
	return 1;
}

void AMCXPOrb::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	if (!Mesh->GetStaticMesh())
	{
		Mesh->SetStaticMesh(MCAssets::Sphere());
		Mesh->SetMaterial(0, MCAssets::MakeMID(Mesh, MCAssets::GlowMaterial(), FLinearColor(0.6f, 1.f, 0.2f), 4.f));
		const float S = 0.08f + FMath::Min(0.12f, FMath::Loge((float)Value + 1.f) * 0.02f);
		Mesh->SetRelativeScale3D(FVector(S));
	}
	Vel.Z -= Gravity;
	if (bInWater) Vel.Z = FMath::Min(Vel.Z + 0.03, 0.06);
	// attraction towards the nearest player within 8 blocks
	if (Game && Game->Player && Game->Player->World == World && !Game->Player->IsSpectator())
	{
		AMCPlayer* P = Game->Player;
		const FVector To = P->Pos + FVector(0, 0, P->EyeHeight / 2.0) - Pos;
		const double D = To.Size() / 8.0;
		if (D < 1.0)
		{
			const double F = 1.0 - D;
			Vel += To.GetSafeNormal() * F * F * 0.1;
		}
	}
	Move(Vel);
	double Fr = 0.98;
	if (bOnGround) Fr = FMCBlocks::GetByState(World->GetState(BlockBelow())).Friction * 0.98;
	Vel.X *= Fr; Vel.Y *= Fr; Vel.Z *= 0.98;
	if (bOnGround) Vel.Z *= -0.9;
	if (++Lifetime >= 6000 + 6000) Discard();
}

void AMCXPOrb::UpdateVisual(float Alpha, float DeltaSeconds)
{
	Super::UpdateVisual(Alpha, DeltaSeconds);
	if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
	{
		const float T = (Age + Alpha) / 2.f;
		const float R = (FMath::Sin(T) + 1.f) * 0.5f;
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(R, 1.f, (FMath::Sin(T + 4.1888f) + 1.f) * 0.1f));
	}
}

void AMCXPOrb::OnPlayerTouch(AMCPlayer* Player)
{
	if (!Player || bRemoved || Age < 2 || Player->Health <= 0.f) return;
	int32 V = Value;
	// mending repairs items first (2 durability per xp point)
	for (int32 s : { Player->Selected, (int32)MCInv::Offhand, 36, 37, 38, 39 })
	{
		FMCItemStack& S = Player->Inventory.Slots[s];
		if (S.IsEmpty() || S.Damage <= 0 || S.GetEnchant(EMCEnchant::Mending) == 0) continue;
		const int32 Repair = FMath::Min(V * 2, S.Damage);
		S.Damage -= Repair;
		V -= Repair / 2;
		if (V <= 0) break;
	}
	if (V > 0) Player->GiveXP(V);
	Player->PlaySound(TEXT("experience_orb_pickup"), 0.1f, 0.5f * ((Rand().NextFloat() - Rand().NextFloat()) * 0.7f + 1.8f));
	Discard();
}

void AMCXPOrb::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Value << Lifetime;
}

// =====================================================================================================================
// Falling block

AMCFallingBlock::AMCFallingBlock()
{
	Kind = EMCEntityKind::FallingBlock;
	TypeId = TEXT("falling_block");
	Width = Height = 0.98f;
	Gravity = 0.04;
	StepHeight = 0.f;
	BlockVisual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("BlockVisual"));
	BlockVisual->SetupAttachment(VisualRoot);
	BlockVisual->SetRelativeLocation(FVector(0, 0, 50));
}

void AMCFallingBlock::InitEntity()
{
	if (State) BlockVisual->SetBlockState(State, 1.f);
	const FName N = FMCBlocks::GetByState(State).Name;
	bHurtEntities = N.ToString().Contains(TEXT("anvil")) || N == TEXT("pointed_dripstone");
}

void AMCFallingBlock::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	if (State == 0) { Discard(); return; }
	if (BlockVisual->IsEmptyVisual()) BlockVisual->SetBlockState(State, 1.f);
	++FallTime;
	Vel.Z -= Gravity;
	const double FallStartZ = Pos.Z;
	Move(Vel);
	Vel *= 0.98;
	const FMCBlock& B = FMCBlocks::GetByState(State);
	// concrete powder hardens in water
	if (B.Name.ToString().EndsWith(TEXT("_concrete_powder")) && bInWater)
	{
		const FName Hard(*B.Name.ToString().LeftChop(7));
		if (const FMCBlock* HB = FMCBlocks::Find(Hard)) State = HB->BaseState;
	}
	if (bOnGround)
	{
		const FMCBlockPos P = BlockPos();
		const FMCState At = World->GetState(P);
		if (FMCBlocks::IsReplaceable(At) && B.Behavior->CanSurvive(B, *World, P, State))
		{
			if (At != 0) World->DestroyBlock(P, true, nullptr, nullptr, false);
			World->SetState(P, State, MCSet_Default);
			World->PlayBlockSound(State, 1, FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5));
			if (B.Name.ToString().Contains(TEXT("anvil"))) World->PlaySound(TEXT("anvil_land"), FVector(P.X + 0.5, P.Y + 0.5, P.Z), 0.3f, 1.f);
		}
		else if (!Game || Game->Rules.bDoTileDrops)
		{
			const FMCItemId Id = FMCItems::ForBlock(B.Id);
			if (Id) World->SpawnItem(Pos + FVector(0, 0, 0.5), FMCItemStack(Id, 1));
		}
		Discard();
		return;
	}
	if (bHurtEntities && FallDistance > 1.f)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(GetBox(), Near, this);
		for (AMCEntity* E : Near)
		{
			if (!E->IsLiving()) continue;
			const float Dmg = FMath::Min(40.f, FMath::CeilToFloat(FallDistance - 1.f) * 2.f);
			E->Hurt(FMCDamage::Of(B.Name == TEXT("pointed_dripstone") ? TEXT("falling_stalactite") : TEXT("falling_anvil")), Dmg);
		}
	}
	(void)FallStartZ;
	if (FallTime > 600 || Pos.Z < MC::MinZ - 64) Discard();
}

void AMCFallingBlock::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << State << FallTime;
}

// =====================================================================================================================
// Primed TNT

AMCPrimedTNT::AMCPrimedTNT()
{
	Kind = EMCEntityKind::TNT;
	TypeId = TEXT("tnt");
	Width = Height = 0.98f;
	Gravity = 0.04;
	StepHeight = 0.f;
	BlockVisual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("BlockVisual"));
	BlockVisual->SetupAttachment(VisualRoot);
	BlockVisual->SetRelativeLocation(FVector(0, 0, 50));
}

void AMCPrimedTNT::InitEntity()
{
	BlockVisual->SetBlockState(FMCBlocks::FindState(TEXT("tnt")), 1.f);
	if (Vel.IsZero())
	{
		const float A = Rand().NextFloat() * 2.f * PI;
		Vel = FVector(-FMath::Sin(A) * 0.02, FMath::Cos(A) * 0.02, 0.2);
	}
	PlaySound(TEXT("tnt_primed"), 1.f, 1.f);
}

void AMCPrimedTNT::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	Vel.Z -= Gravity;
	Move(Vel);
	Vel *= 0.98;
	if (bOnGround) { Vel.X *= 0.7; Vel.Y *= 0.7; Vel.Z *= -0.5; }
	if (--Fuse <= 0)
	{
		Discard();
		World->Explode(Pos + FVector(0, 0, 0.0625), Power, false, true, this);
		return;
	}
	World->SpawnParticles(TEXT("smoke"), Pos + FVector(0, 0, 1.05), 1, 0.05f);
}

void AMCPrimedTNT::UpdateVisual(float Alpha, float DeltaSeconds)
{
	Super::UpdateVisual(Alpha, DeltaSeconds);
	const float F = Fuse - Alpha + 1.f;
	float S = 1.f;
	if (F < 10.f) { const float K = FMath::Clamp(1.f - F / 10.f, 0.f, 1.f); S = 1.f + K * K * K * K * 0.3f; }
	VisualRoot->SetRelativeScale3D(FVector(S));
	BlockVisual->SetFlash(Fuse / 5 % 2 == 0 ? 0.8f : 0.f);
	BlockVisual->SetHiddenAll((Fuse / 5) % 2 == 0 && Fuse < 60 ? false : false);
}

void AMCPrimedTNT::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Fuse << Power;
}

// =====================================================================================================================
// End crystal

AMCEndCrystal::AMCEndCrystal()
{
	Kind = EMCEntityKind::EndCrystal;
	TypeId = TEXT("end_crystal");
	Width = Height = 2.f;
	bNoGravity = true;
	bFireImmune = true;
	bPersistent = true;
	auto MakeMesh = [&](const TCHAR* Name) { UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name); M->SetupAttachment(VisualRoot); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); return M; };
	Core = MakeMesh(TEXT("Core"));
	Cage = MakeMesh(TEXT("Cage"));
	Beam = MakeMesh(TEXT("Beam"));
	Base = MakeMesh(TEXT("Base"));
	Beam->SetCastShadow(false);
	Beam->SetVisibility(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(VisualRoot);
	Light->SetRelativeLocation(FVector(0, 0, 110));
	Light->SetIntensity(3000.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.9f));
	Light->SetCastShadows(false);
}

void AMCEndCrystal::TickEntity()
{
	Super::TickEntity();
	if (!Core->GetStaticMesh())
	{
		UStaticMesh* Authored = MCAssets::Mesh(TEXT("/Game/Opus55Minecraft/Entities/SM_EndCrystal.SM_EndCrystal"));
		Core->SetStaticMesh(Authored ? Authored : MCAssets::Cube());
		Core->SetMaterial(0, MCAssets::MakeMID(Core, Authored ? MCAssets::EntityMaterial() : MCAssets::GlowMaterial(), FLinearColor(0.9f, 0.4f, 1.f), Authored ? 0.5f : 6.f));
		Core->SetRelativeScale3D(Authored ? FVector(1.f) : FVector(0.45f));
		Core->SetRelativeLocation(FVector(0, 0, 120));
		Cage->SetStaticMesh(MCAssets::Cube());
		Cage->SetMaterial(0, MCAssets::MakeMID(Cage, MCAssets::BeamMaterial(), FLinearColor(0.8f, 0.8f, 1.f), 1.5f));
		Cage->SetRelativeScale3D(FVector(0.85f));
		Cage->SetRelativeLocation(FVector(0, 0, 120));
		Cage->SetVisibility(Authored == nullptr);
		Base->SetStaticMesh(MCAssets::Cube());
		Base->SetMaterial(0, MCAssets::MakeMID(Base, MCAssets::EntityMaterial(), FLinearColor(0.1f, 0.1f, 0.1f), 0.f));
		Base->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.4f));
		Base->SetRelativeLocation(FVector(0, 0, 20));
		Beam->SetStaticMesh(MCAssets::Cylinder());
		Beam->SetMaterial(0, MCAssets::MakeMID(Beam, MCAssets::BeamMaterial(), FLinearColor(1.f, 0.6f, 1.f), 4.f));
	}
	Base->SetVisibility(bShowBottom);
	if (World && World->Dim == EMCDimension::End && bSpike)
	{
		// crystals keep a fire beneath them on the pillars
		const FMCBlockPos P = BlockPos();
		if (World->GetState(P) == 0 && FMCBlocks::IsSolid(World->GetState(P.Down()))) World->SetState(P, FMCBlocks::C.Fire, MCSet_Default);
	}
	if (!bHasBeam) BeamTarget = FVector::ZeroVector;
	bHasBeam = false || !BeamTarget.IsZero();
}

void AMCEndCrystal::UpdateVisual(float Alpha, float DeltaSeconds)
{
	Super::UpdateVisual(Alpha, DeltaSeconds);
	SetActorRotation(FRotator::ZeroRotator);
	Spin += DeltaSeconds * 120.f;
	const float Bob = FMath::Sin((Age + Alpha) * 0.2f) * 0.25f;
	Core->SetRelativeLocation(FVector(0, 0, (1.2f + Bob) * MC::BlockSizeF));
	Core->SetRelativeRotation(FRotator(Spin * 0.7f, Spin, 45.f));
	Cage->SetRelativeLocation(FVector(0, 0, (1.2f + Bob) * MC::BlockSizeF));
	Cage->SetRelativeRotation(FRotator(-Spin * 0.5f, -Spin * 0.8f, 30.f));
	Light->SetRelativeLocation(FVector(0, 0, (1.2f + Bob) * MC::BlockSizeF));
	if (!BeamTarget.IsZero())
	{
		const FVector From = (Pos + FVector(0, 0, 1.2 + Bob)) * MC::BlockSize;
		const FVector To = BeamTarget * MC::BlockSize;
		const FVector D = To - From;
		const double L = D.Size();
		Beam->SetVisibility(true);
		Beam->SetWorldLocation(From + D * 0.5);
		Beam->SetWorldRotation(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
		Beam->SetWorldScale3D(FVector(0.12, 0.12, L / 100.0));
	}
	else Beam->SetVisibility(false);
}

bool AMCEndCrystal::Hurt(const FMCDamage& D, float Amount)
{
	if (bRemoved || !World) return false;
	if (D.Attacker.IsValid() && D.Attacker->IsA<AMCEnderDragon>()) return false;
	Discard();
	if (!D.bExplosion || true) World->Explode(Pos + FVector(0, 0, 0.5), 6.f, false, true, this);
	if (Game) Game->TickDragonFight();
	return true;
}

void AMCEndCrystal::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << bShowBottom << bSpike;
}

// =====================================================================================================================
// Lightning

AMCLightning::AMCLightning()
{
	Kind = EMCEntityKind::Lightning;
	TypeId = TEXT("lightning_bolt");
	bNoGravity = true;
	bNoPhysics = true;
	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(VisualRoot);
	Flash->SetRelativeLocation(FVector(0, 0, 800));
	Flash->SetIntensity(200000.f);
	Flash->SetAttenuationRadius(12000.f);
	Flash->SetLightColor(FLinearColor(0.75f, 0.8f, 1.f));
	Flash->SetCastShadows(true);
	Bolt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bolt"));
	Bolt->SetupAttachment(VisualRoot);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetCastShadow(false);
}

void AMCLightning::InitEntity()
{
	Life = 2;
	Flashes = 1 + Rand().NextInt(3);
	Bolt->SetStaticMesh(MCAssets::Cylinder());
	Bolt->SetMaterial(0, MCAssets::MakeMID(Bolt, MCAssets::GlowMaterial(), FLinearColor(0.7f, 0.75f, 1.f), 40.f));
	Bolt->SetRelativeScale3D(FVector(0.06f, 0.06f, 256.f));
	Bolt->SetRelativeLocation(FVector(0, 0, 12800));
	if (!World) return;
	World->PlaySound(TEXT("lightning_bolt_thunder"), Pos, 10000.f, 0.8f + Rand().NextFloat() * 0.2f);
	World->PlaySound(TEXT("lightning_bolt_impact"), Pos, 2.f, 0.5f + Rand().NextFloat() * 0.2f);
	if (bVisualOnly) return;
	// fire at the strike point
	if (Game && Game->Rules.bDoFireTick && Game->Difficulty >= EMCDifficulty::Normal)
	{
		const FMCBlockPos P = BlockPos();
		if (World->GetState(P) == 0 && FMCBlocks::IsSolid(World->GetState(P.Down()))) World->SetState(P, FMCBlocks::C.Fire, MCSet_Default);
	}
	// lightning rods / copper de-oxidation handled by behaviours via OnProjectileHit-like hook
	const FMCBlockPos Rod = BlockPos().Down();
	const FMCState RS = World->GetState(Rod);
	if (FMCBlocks::GetByState(RS).Name == TEXT("lightning_rod")) FMCBlocks::GetByState(RS).Behavior->OnProjectileHit(*World, Rod, RS, this, Pos);
	TArray<AMCEntity*> Near;
	World->GetEntitiesInBox(FMCBox(Pos - FVector(3, 3, 3), Pos + FVector(3, 3, 6)), Near, this);
	for (AMCEntity* E : Near)
	{
		if (E->bRemoved) continue;
		E->OnLightning();
		if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("lightning_bolt")), 5.f);
		else if (E->IsA<AMCItemEntity>()) E->Hurt(FMCDamage::Of(TEXT("in_fire")), 1.f);
	}
}

void AMCLightning::TickEntity()
{
	PrevPos = Pos;
	++Age;
	--Life;
	if (Life < 0)
	{
		if (Flashes <= 0) { Discard(); return; }
		if (Life < -Rand().NextInt(10)) { --Flashes; Life = 1; }
	}
	if (Game) Game->LightningFlash = 1.f;
}

void AMCLightning::UpdateVisual(float Alpha, float DeltaSeconds)
{
	Super::UpdateVisual(Alpha, DeltaSeconds);
	const bool bOn = Life >= 0;
	Bolt->SetVisibility(bOn);
	Flash->SetVisibility(bOn);
	// jitter the bolt a little each frame
	Bolt->SetRelativeRotation(FRotator(FMath::FRandRange(-1.5f, 1.5f), 0.f, FMath::FRandRange(-1.5f, 1.5f)));
}

// =====================================================================================================================
// Item frame

AMCItemFrame::AMCItemFrame()
{
	Kind = EMCEntityKind::ItemFrame;
	TypeId = TEXT("item_frame");
	Width = 0.75f; Height = 0.75f;
	bNoGravity = true;
	bNoPhysics = true;
	bPersistent = true;
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(VisualRoot);
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemVisual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("ItemVisual"));
	ItemVisual->SetupAttachment(VisualRoot);
}

void AMCItemFrame::InitEntity()
{
	UStaticMesh* Authored = MCAssets::Mesh(TEXT("/Game/Opus55Minecraft/Entities/SM_ItemFrame.SM_ItemFrame"));
	Frame->SetStaticMesh(Authored ? Authored : MCAssets::Cube());
	Frame->SetMaterial(0, MCAssets::MakeMID(Frame, MCAssets::EntityMaterial(), bGlow ? FLinearColor(0.4f, 0.9f, 0.8f) : FLinearColor(0.55f, 0.36f, 0.2f), bGlow ? 1.f : 0.f));
	if (!Authored) Frame->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.0625f));
	ItemVisual->SetStack(Item, 3);
}

void AMCItemFrame::TickEntity()
{
	PrevPos = Pos;
	++Age;
	if (!World || Age % 20 != 0) return;
	// pop off when the supporting block disappears
	const FMCBlockPos Support = FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z + 0.5)).Offset(MC::Opposite(Facing));
	if (!FMCBlocks::IsSolid(World->GetState(Support)))
	{
		World->SpawnItem(Pos + FVector(0, 0, 0.5), FMCItemStack::Of(bGlow ? TEXT("glow_item_frame") : TEXT("item_frame"), 1));
		if (!Item.IsEmpty()) World->SpawnItem(Pos + FVector(0, 0, 0.5), Item);
		Discard();
	}
}

void AMCItemFrame::UpdateVisual(float Alpha, float DeltaSeconds)
{
	// flush against the supporting face
	const FVector N(MC::FaceDir[(int32)Facing]);
	const FVector Center = Pos + FVector(0, 0, 0.5) - N * (0.5 - 0.03125);
	SetActorLocation(Center * MC::BlockSize);
	FRotator R = FRotationMatrix::MakeFromZ(N).Rotator();
	SetActorRotation(R);
	ItemVisual->SetRelativeLocation(FVector(0, 0, 4.0));
	ItemVisual->SetRelativeRotation(FRotator(0, 0, 0) + FRotator(0.f, ItemRotation * 45.f, 0.f) + FRotator(90.f, 0.f, 0.f));
	ItemVisual->SetRelativeScale3D(FVector(0.5f));
}

bool AMCItemFrame::Interact(AMCPlayer* Player, bool bOffHand)
{
	if (!Player) return false;
	if (Item.IsEmpty())
	{
		FMCItemStack& H = bOffHand ? Player->Inventory.Slots[MCInv::Offhand] : Player->Held();
		if (H.IsEmpty()) return false;
		Item = H.Copy(); Item.Count = 1;
		if (!Player->IsCreative()) { H.Count -= 1; if (H.Count <= 0) H.Clear(); }
		ItemVisual->SetStack(Item, 3);
		PlaySound(TEXT("item_frame_add_item"), 1.f, 1.f);
		return true;
	}
	ItemRotation = (ItemRotation + 1) % 8;
	PlaySound(TEXT("item_frame_rotate_item"), 1.f, 1.f);
	if (World) World->NotifyNeighbors(BlockPos());
	return true;
}

bool AMCItemFrame::Hurt(const FMCDamage& D, float Amount)
{
	if (!World || bRemoved) return false;
	const AMCPlayer* P = Cast<AMCPlayer>(D.Attacker.Get());
	if (!Item.IsEmpty())
	{
		if (!(P && P->IsCreative())) World->SpawnItem(Pos + FVector(0, 0, 0.5), Item);
		Item.Clear();
		ItemVisual->Clear();
		PlaySound(TEXT("item_frame_remove_item"), 1.f, 1.f);
		return true;
	}
	if (!(P && P->IsCreative())) World->SpawnItem(Pos + FVector(0, 0, 0.5), FMCItemStack::Of(bGlow ? TEXT("glow_item_frame") : TEXT("item_frame"), 1));
	PlaySound(TEXT("item_frame_break"), 1.f, 1.f);
	Discard();
	return true;
}

void AMCItemFrame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	uint8 F = (uint8)Facing; Ar << F; if (Ar.IsLoading()) Facing = (EMCFace)F;
	Ar << Item << ItemRotation << bGlow;
	if (Ar.IsLoading()) InitEntity();
}

// =====================================================================================================================
// Eye of ender

AMCEyeOfEnder::AMCEyeOfEnder()
{
	Kind = EMCEntityKind::EyeOfEnder;
	TypeId = TEXT("eye_of_ender");
	Width = Height = 0.25f;
	bNoPhysics = true;
	bNoGravity = true;
	Visual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("ItemModel")); // "Visual" is AMCEntity::VisualRoot
	Visual->SetupAttachment(VisualRoot);
}

void AMCEyeOfEnder::TickEntity()
{
	PrevPos = Pos;
	++Age; ++Life;
	if (Life == 1) { Visual->SetStack(FMCItemStack::Of(TEXT("ender_eye"), 1), 0); Visual->SetRelativeScale3D(FVector(0.5f)); }
	if (!World) return;
	// fly towards the stronghold (horizontal), rising then sinking
	FVector D = TargetPos - Pos; D.Z = 0;
	const double Dist = D.Size();
	FVector H = Dist > 1e-3 ? D / Dist : FVector::ZeroVector;
	const double Speed = FMath::Min(Dist, 12.0) * 0.0025 + 0.1;
	const double TargetZ = Dist < 1.0 ? Pos.Z - 0.2 : (Pos.Z + (Life < 40 ? 0.04 : -0.01));
	Vel = FVector(H.X * Speed * 2.5, H.Y * Speed * 2.5, (TargetZ - Pos.Z));
	Pos += Vel;
	World->SpawnParticles(TEXT("portal"), Pos, 2, 0.1f, -Vel * 0.5, FColor(150, 60, 220));
	if (Life >= 80)
	{
		if (bSurvive) World->SpawnItem(Pos, FMCItemStack::Of(TEXT("ender_eye"), 1), false, 0.f);
		else { World->SpawnParticles(TEXT("item_break"), Pos, 12, 0.2f, FVector(0, 0, 0.1), FColor(40, 140, 100)); World->PlaySound(TEXT("ender_eye_death"), Pos, 1.f, 1.f); }
		Discard();
	}
}

// =====================================================================================================================
// Firework rocket

AMCFirework::AMCFirework()
{
	Kind = EMCEntityKind::Firework;
	TypeId = TEXT("firework_rocket");
	Width = Height = 0.25f;
	bNoGravity = true;
	Visual = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("ItemModel")); // "Visual" is AMCEntity::VisualRoot
	Visual->SetupAttachment(VisualRoot);
}

void AMCFirework::TickEntity()
{
	PrevPos = Pos;
	++Age;
	if (!World) return;
	if (Life == 0) { Visual->SetStack(FMCItemStack::Of(TEXT("firework_rocket"), 1), 0); Visual->SetRelativeScale3D(FVector(0.4f)); PlaySound(TEXT("firework_rocket_launch"), 3.f, 1.f); }
	++Life;
	if (AMCLiving* L = Cast<AMCLiving>(AttachedTo.Get()))
	{
		// elytra boost
		if (L->bElytraFlying)
		{
			const FVector Look = L->GetLookDir();
			L->Vel += Look * 0.1 + (Look * 1.5 - L->Vel) * 0.5;
		}
		Pos = L->Pos;
	}
	else
	{
		if (Vel.IsNearlyZero()) Vel = FVector(Rand().FRange(-0.001f, 0.001f), Rand().FRange(-0.001f, 0.001f), 0.05);
		Vel.X *= 1.15; Vel.Y *= 1.15; Vel.Z += 0.04;
		Move(Vel);
		if (bHorizontalCollision || bVerticalCollision) Life = LifeTime;
	}
	World->SpawnParticles(TEXT("firework_spark"), Pos, 1, 0.02f, -Vel * 0.3, FColor(255, 230, 180));
	if (Life >= LifeTime) Explode();
}

void AMCFirework::Explode()
{
	if (!World || bRemoved) return;
	const uint8 C = Item.Extra.IsValid() ? Item.Extra->Color : 255;
	static const FColor Cols[] = { FColor(255, 80, 80), FColor(80, 180, 255), FColor(255, 220, 60), FColor(120, 255, 120), FColor(255, 120, 255), FColor(255, 255, 255) };
	const FColor Col = C < 16 ? FColor(255, 255, 255) : Cols[Rand().NextInt(UE_ARRAY_COUNT(Cols))];
	World->SpawnParticles(TEXT("firework_burst"), Pos, 90, 0.f, FVector::ZeroVector, Col);
	World->PlaySound(Rand().NextBool() ? TEXT("firework_rocket_blast") : TEXT("firework_rocket_large_blast"), Pos, 20.f, 0.95f + Rand().NextFloat() * 0.1f);
	World->PlaySound(TEXT("firework_rocket_twinkle"), Pos, 20.f, 0.95f + Rand().NextFloat() * 0.1f);
	// rockets with stars damage nearby entities
	if (Item.Extra.IsValid() && Item.Extra->Charge > 0)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(GetBox().Inflate(5.0), Near, this);
		for (AMCEntity* E : Near) if (E->IsLiving()) E->Hurt(FMCDamage::Of(TEXT("fireworks")), 5.f + Item.Extra->Charge * 2.f);
	}
	Discard();
}

// =====================================================================================================================
// Lingering potion / dragon breath cloud

AMCAreaCloud::AMCAreaCloud()
{
	Kind = EMCEntityKind::AreaCloud;
	TypeId = TEXT("area_effect_cloud");
	bNoGravity = true;
	bNoPhysics = true;
	Width = 6.f; Height = 0.5f;
}

void AMCAreaCloud::TickEntity()
{
	PrevPos = Pos;
	++Age;
	if (!World) return;
	Width = Radius * 2.f;
	// particles filling the disc
	const int32 N = FMath::Clamp((int32)(Radius * Radius * 0.6f), 2, 40);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = Rand().NextFloat() * 2.f * PI;
		const float R = FMath::Sqrt(Rand().NextFloat()) * Radius;
		World->SpawnParticles(bDragonBreath ? TEXT("dragon_breath") : TEXT("effect"), Pos + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.1), 1, 0.f, FVector(0, 0, 0.02), Color);
	}
	if (Age % 5 == 0)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(FMCBox(Pos - FVector(Radius, Radius, 0.5), Pos + FVector(Radius, Radius, 2.0)), Near, this);
		for (AMCEntity* E : Near)
		{
			AMCLiving* L = Cast<AMCLiving>(E);
			if (!L || !L->IsAlive() || L == Owner.Get() || (bDragonBreath && L->IsA<AMCEnderDragon>())) continue;
			if (FVector::Dist2D(L->Pos, Pos) > Radius) continue;
			if (bDragonBreath)
			{
				FMCDamage D = FMCDamage::Of(TEXT("dragon_breath"));
				D.Attacker = Owner;
				L->Hurt(D, 6.f);
			}
			else if (Effect != EMCEffect::None)
			{
				FMCEffectInstance I; I.Effect = Effect; I.Duration = EffectDuration / 4; I.Amplifier = Amplifier;
				L->AddEffect(I);
			}
			Radius -= 0.5f * 0.25f;
		}
	}
	Radius -= Radius / FMath::Max(1, Duration) * 0.5f;
	if (Age >= Duration || Radius < 0.5f) Discard();
}

void AMCAreaCloud::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	uint8 E = (uint8)Effect; Ar << E; if (Ar.IsLoading()) Effect = (EMCEffect)E;
	Ar << Radius << Duration << EffectDuration << Amplifier << Color << bDragonBreath;
}
