// Vehicles: boats (with chests / bamboo rafts) and minecarts (rail physics, powered rails, chest / furnace / hopper / TNT carts).
#include "Game/MCEntities.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCGame.h"
#include "Game/MCMenu.h"
#include "Render/MCRig.h"
#include "Render/MCAssets.h"
#include "Render/MCChunkMeshComponent.h"
#include "Blocks/MCTextures.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlockBehavior.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	/** Box-built vehicle geometry textured with terrain layers (used when no authored mesh exists). */
	struct FBoxModel
	{
		TUniquePtr<FMCChunkMeshData> Data = MakeUnique<FMCChunkMeshData>();
		void Box(const FVector3f& Min, const FVector3f& Max, int16 Tex, const FColor& Tint = FColor::White)
		{
			FMCMeshLayerData& L = Data->Layers[0];
			const FVector3f C[8] = { {Min.X, Min.Y, Min.Z}, {Max.X, Min.Y, Min.Z}, {Max.X, Max.Y, Min.Z}, {Min.X, Max.Y, Min.Z},
				{Min.X, Min.Y, Max.Z}, {Max.X, Min.Y, Max.Z}, {Max.X, Max.Y, Max.Z}, {Min.X, Max.Y, Max.Z} };
			const int32 F[6][4] = { {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {3, 0, 4, 7}, {1, 2, 6, 5} };
			const FVector3f N[6] = { {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0} };
			const FVector3f Size = Max - Min;
			for (int32 f = 0; f < 6; ++f)
			{
				const int32 Base = L.Vertices.Num();
				for (int32 k = 0; k < 4; ++k)
				{
					FMCMeshVertex V;
					V.Pos = C[F[f][k]] * MC::BlockSizeF;
					V.Normal = N[f];
					V.Tangent = FMath::Abs(N[f].Z) > 0.5f ? FVector3f(1, 0, 0) : FVector3f(0, 0, 1);
					const float U = (k == 1 || k == 2) ? 1.f : 0.f, Vv = (k >= 2) ? 0.f : 1.f;
					const float SU = (f < 2) ? Size.X : (f < 4 ? Size.X : Size.Y);
					const float SV = (f < 2) ? Size.Y : Size.Z;
					V.UV0 = FVector2f(U * FMath::Max(SU, 0.1f), Vv * FMath::Max(SV, 0.1f));
					V.UV1 = FVector2f((float)Tex, (float)(Tint != FColor::White ? MCRender::VF_Tint : 0));
					V.UV2 = FVector2f(1.f, 0.f);
					V.Color = FColor(Tint.R, Tint.G, Tint.B, 255);
					L.Vertices.Add(V);
				}
				L.Indices.Append({ (uint32)Base, (uint32)Base + 2, (uint32)Base + 1, (uint32)Base, (uint32)Base + 3, (uint32)Base + 2 });
			}
			Data->Bounds += FBox(FVector(Min) * MC::BlockSize, FVector(Max) * MC::BlockSize);
		}
	};

	UMCChunkMeshComponent* MakeBoxComponent(AActor* Owner, USceneComponent* Parent, TUniquePtr<FMCChunkMeshData>&& Data)
	{
		UMCChunkMeshComponent* C = NewObject<UMCChunkMeshComponent>(Owner);
		C->bRetainData = true;   // nobody re-meshes vehicle parts after a render-state recreation
		C->SharedMaterials = MCRender::GVoxelMaterials;
		C->SetupAttachment(Parent);
		C->SetCastShadow(true);
		C->RegisterComponent();
		C->SetMeshData(MoveTemp(Data));
		return C;
	}

	void GetRailExitsFor(int32 Shape, FIntVector& A, FIntVector& B)
	{
		switch (Shape)
		{
		case 0: A = FIntVector(0, -1, 0); B = FIntVector(0, 1, 0); break;   // north-south
		case 1: A = FIntVector(-1, 0, 0); B = FIntVector(1, 0, 0); break;   // east-west
		case 2: A = FIntVector(-1, 0, 0); B = FIntVector(1, 0, 1); break;   // ascending east
		case 3: A = FIntVector(-1, 0, 1); B = FIntVector(1, 0, 0); break;   // ascending west
		case 4: A = FIntVector(0, -1, 1); B = FIntVector(0, 1, 0); break;   // ascending north
		case 5: A = FIntVector(0, -1, 0); B = FIntVector(0, 1, 1); break;   // ascending south
		case 6: A = FIntVector(0, 1, 0); B = FIntVector(1, 0, 0); break;    // south-east
		case 7: A = FIntVector(0, 1, 0); B = FIntVector(-1, 0, 0); break;   // south-west
		case 8: A = FIntVector(0, -1, 0); B = FIntVector(-1, 0, 0); break;  // north-west
		default: A = FIntVector(0, -1, 0); B = FIntVector(1, 0, 0); break;  // north-east
		}
	}

	bool IsRail(FMCState S) { return FMCBlocks::GetByState(S).Model == EMCModel::Rail; }
	int32 RailShape(FMCState S)
	{
		const FName N = FMCBlocks::GetByState(S).Name;
		const uint8 M = FMCBlocks::MetaOf(S);
		return (N == TEXT("rail")) ? FMath::Min<int32>(M, 9) : (M & 7);
	}
}

// =====================================================================================================================
// Boat

AMCBoat::AMCBoat()
{
	Kind = EMCEntityKind::Boat;
	TypeId = TEXT("boat");
	Width = 1.375f; Height = 0.5625f;
	Gravity = 0.04;
	StepHeight = 0.f;
	bPersistent = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(VisualRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FName AMCBoat::ItemName() const
{
	if (Wood == TEXT("bamboo")) return bChest ? FName(TEXT("bamboo_chest_raft")) : FName(TEXT("bamboo_raft"));
	return FName(*FString::Printf(TEXT("%s_%s"), *Wood.ToString(), bChest ? TEXT("chest_boat") : TEXT("boat")));
}

void AMCBoat::InitEntity()
{
	if (bChest && Chest.Num() != 27) Chest.Init(27);
	UStaticMesh* Authored = MCAssets::Mesh(*FString::Printf(TEXT("/Game/Opus55Minecraft/Entities/SM_Boat_%s.SM_Boat_%s"), *Wood.ToString(), *Wood.ToString()));
	if (!Authored) Authored = MCAssets::Mesh(TEXT("/Game/Opus55Minecraft/Entities/SM_Boat.SM_Boat"));
	if (Authored)
	{
		Mesh->SetStaticMesh(Authored);
		const FColor WoodCol = FMCTextures::AverageColor(FMCTextures::Find(FName(*(Wood.ToString() + TEXT("_planks")))));
		Mesh->SetMaterial(0, MCAssets::MakeMID(Mesh, MCAssets::EntityMaterial(), FLinearColor(WoodCol), 0.f));
	}
	else if (VisualRoot->GetNumChildrenComponents() <= 1)
	{
		const int16 Planks = FMCTextures::Find(FName(*(Wood.ToString() + TEXT("_planks"))));
		FBoxModel M;
		// hull: bottom + four sides (MC boat is 1.375 x 1.375 footprint with low walls)
		M.Box(FVector3f(-0.7f, -0.45f, 0.0f), FVector3f(0.7f, 0.45f, 0.1f), Planks);
		M.Box(FVector3f(-0.7f, -0.5f, 0.0f), FVector3f(0.7f, -0.42f, 0.4f), Planks);
		M.Box(FVector3f(-0.7f, 0.42f, 0.0f), FVector3f(0.7f, 0.5f, 0.4f), Planks);
		M.Box(FVector3f(0.62f, -0.45f, 0.0f), FVector3f(0.8f, 0.45f, 0.35f), Planks);
		M.Box(FVector3f(-0.8f, -0.45f, 0.0f), FVector3f(-0.62f, 0.45f, 0.35f), Planks);
		M.Box(FVector3f(-0.1f, -0.42f, 0.2f), FVector3f(0.1f, 0.42f, 0.28f), Planks); // seat
		if (bChest) M.Box(FVector3f(-0.65f, -0.35f, 0.1f), FVector3f(-0.1f, 0.35f, 0.65f), FMCTextures::Find(TEXT("chest_side")), FColor(200, 150, 90));
		MakeBoxComponent(this, VisualRoot, MoveTemp(M.Data));
		Mesh->SetVisibility(false);
	}
}

FVector AMCBoat::GetPassengerOffset(const AMCEntity* Passenger) const
{
	const int32 Idx = Passengers.IndexOfByPredicate([&](const TWeakObjectPtr<AMCEntity>& P) { return P.Get() == Passenger; });
	const FVector Fwd = FRotator(0, Yaw, 0).Vector();
	double Along = 0.0;
	if (Passengers.Num() > 1 || bChest) Along = Idx == 0 ? 0.2 : -0.6;
	return Fwd * Along + FVector(0, 0, Passenger && Passenger->IsA<AMCPlayer>() ? 0.1 : 0.2);
}

void AMCBoat::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	if (HurtTicks > 0) --HurtTicks;
	if (Damage > 0.f) Damage = FMath::Max(0.f, Damage - 1.f);

	// water state
	double Surface = 0.0;
	const bool bWater = World->IsInFluid(GetBox().Inflate(-0.05).Offset(0, 0, -0.1), FMCBlocks::C.WaterId, &Surface);
	const bool bUnder = bWater && Surface > Pos.Z + Height + 0.1;
	float Friction = 0.05f;
	if (bWater && !bUnder)
	{
		Friction = 0.9f;
		const double Target = Surface - 0.35;
		Vel.Z += (Target - Pos.Z) * 0.06153846 * 3.0;
		Vel.Z *= 0.75;
	}
	else if (bUnder)
	{
		Vel.Z += 0.01;
		Friction = 0.45f;
	}
	else
	{
		Vel.Z -= Gravity;
		if (bOnGround)
		{
			const FName Below = FMCBlocks::GetByState(World->GetState(BlockBelow())).Name;
			Friction = (Below == TEXT("ice") || Below == TEXT("packed_ice") || Below == TEXT("frosted_ice")) ? 0.98f : (Below == TEXT("blue_ice") ? 0.989f : 0.45f);
		}
		else Friction = 0.9f;
	}

	// driver controls
	AMCPlayer* Driver = Passengers.Num() > 0 ? Cast<AMCPlayer>(Passengers[0].Get()) : nullptr;
	if (Driver)
	{
		const float Fwd = Driver->Input.Forward;
		const float Side = Driver->Input.Strafe;
		float Accel = 0.f;
		if (Side < -0.1f) DeltaYaw -= 1.f;
		if (Side > 0.1f) DeltaYaw += 1.f;
		if (FMath::Abs(Side) > 0.1f && FMath::Abs(Fwd) < 0.1f) Accel += 0.005f;
		Yaw += DeltaYaw;
		if (Fwd > 0.1f) Accel += 0.04f;
		if (Fwd < -0.1f) Accel -= 0.005f;
		const FVector F = FRotator(0, Yaw, 0).Vector();
		Vel.X += F.X * Accel; Vel.Y += F.Y * Accel;
		PaddleL += Accel != 0.f || Side > 0.1f ? 0.39f : 0.f;
		PaddleR += Accel != 0.f || Side < -0.1f ? 0.39f : 0.f;
		if (Accel != 0.f && Age % 16 == 0 && bWater) PlaySound(TEXT("boat_paddle_water"), 0.5f, 0.8f + Rand().NextFloat() * 0.4f);
	}
	Vel.X *= Friction; Vel.Y *= Friction;
	DeltaYaw *= Friction;
	Move(Vel);
	// pick up mobs that bump into the boat
	if (Passengers.Num() < (bChest ? 1 : 2) && !bWater == false)
	{
		TArray<AMCEntity*> Near;
		World->GetEntitiesInBox(GetBox().Inflate(0.2), Near, this);
		for (AMCEntity* E : Near)
		{
			AMCMob* M = Cast<AMCMob>(E);
			if (!M || M->IsPassenger() || !M->Def || M->Def->bBoss || M->Width > 1.375f || M->Def->bSwimmer || M->LeashHolder.IsValid()) continue;
			if (Passengers.Num() == 0 && !Driver) continue; // only fill the second seat automatically
			M->StartRiding(this);
			break;
		}
	}
	for (TWeakObjectPtr<AMCEntity>& P : Passengers)
	{
		if (AMCEntity* E = P.Get())
		{
			E->PrevPos = E->Pos;
			E->Pos = Pos + GetPassengerOffset(E);
			if (AMCLiving* L = Cast<AMCLiving>(E)) { if (!L->IsA<AMCPlayer>()) L->Yaw += DeltaYaw; }
		}
	}
}

void AMCBoat::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	const float Y = FMath::Lerp(PrevYaw, PrevYaw + FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Alpha);
	float Roll = HurtTicks > 0 ? FMath::Sin((HurtTicks - Alpha) * 1.2f) * (HurtTicks - Alpha) * Damage / 10.f : 0.f;
	SetActorRotation(FRotator(0.f, Y, Roll));
}

bool AMCBoat::Hurt(const FMCDamage& D, float Amount)
{
	if (bRemoved || !World) return false;
	const AMCPlayer* P = Cast<AMCPlayer>(D.Attacker.Get());
	HurtTicks = 10;
	Damage += Amount * 10.f;
	PlaySound(TEXT("boat_hit"), 1.f, 1.f);
	if ((P && P->IsCreative()) || Damage > 40.f || D.bExplosion || D.bFire)
	{
		if (!(P && P->IsCreative()) && (!Game || Game->Rules.bDoTileDrops))
		{
			World->SpawnItem(Pos + FVector(0, 0, 0.5), FMCItemStack::Of(ItemName(), 1));
			for (FMCItemStack& S : Chest.Slots) if (!S.IsEmpty()) { World->SpawnItem(Pos + FVector(0, 0, 0.5), S); S.Clear(); }
		}
		for (TWeakObjectPtr<AMCEntity>& Ps : Passengers) if (AMCEntity* E = Ps.Get()) E->Vehicle.Reset();
		Passengers.Reset();
		Discard();
	}
	return true;
}

bool AMCBoat::Interact(AMCPlayer* Player, bool bOffHand)
{
	if (!Player || bOffHand) return false;
	if (bChest && Player->bSneaking)
	{
		Player->OpenMenu(MakeShared<FMCChestMenu>(Player, EMCMenuType::Chest, &Chest, nullptr, 3, FString(TEXT("Chest Boat"))));
		return true;
	}
	if (Passengers.Num() >= (bChest ? 1 : 2) || Player->IsPassenger()) return false;
	Player->StartRiding(this);
	Player->ViewYaw = Yaw;
	return true;
}

void AMCBoat::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Wood << bChest;
	Chest.Serialize(Ar);
	if (Ar.IsLoading()) InitEntity();
}

// =====================================================================================================================
// Minecart

AMCMinecart::AMCMinecart()
{
	Kind = EMCEntityKind::Minecart;
	TypeId = TEXT("minecart");
	Width = 0.98f; Height = 0.7f;
	Gravity = 0.04;
	StepHeight = 0.f;
	bPersistent = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(VisualRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Content = CreateDefaultSubobject<UMCItemVisualComponent>(TEXT("Content"));
	Content->SetupAttachment(VisualRoot);
	Content->SetRelativeLocation(FVector(0, 0, 60));
}

void AMCMinecart::InitEntity()
{
	TypeId = Variant;
	if (Variant == TEXT("chest_minecart") && Chest.Num() != 27) Chest.Init(27);
	if (Variant == TEXT("hopper_minecart") && Chest.Num() != 5) Chest.Init(5);
	UStaticMesh* Authored = MCAssets::Mesh(TEXT("/Game/Opus55Minecraft/Entities/SM_Minecart.SM_Minecart"));
	if (Authored)
	{
		Mesh->SetStaticMesh(Authored);
		Mesh->SetMaterial(0, MCAssets::MakeMID(Mesh, MCAssets::EntityMaterial(), FLinearColor(0.55f, 0.55f, 0.6f), 0.f));
	}
	else if (VisualRoot->GetNumChildrenComponents() <= 2)
	{
		const int16 Iron = FMCTextures::Find(TEXT("iron_block"));
		FBoxModel M;
		M.Box(FVector3f(-0.5f, -0.4f, 0.05f), FVector3f(0.5f, 0.4f, 0.15f), Iron, FColor(120, 120, 130));
		M.Box(FVector3f(-0.5f, -0.45f, 0.05f), FVector3f(0.5f, -0.38f, 0.6f), Iron, FColor(110, 110, 120));
		M.Box(FVector3f(-0.5f, 0.38f, 0.05f), FVector3f(0.5f, 0.45f, 0.6f), Iron, FColor(110, 110, 120));
		M.Box(FVector3f(0.43f, -0.4f, 0.05f), FVector3f(0.5f, 0.4f, 0.6f), Iron, FColor(110, 110, 120));
		M.Box(FVector3f(-0.5f, -0.4f, 0.05f), FVector3f(-0.43f, 0.4f, 0.6f), Iron, FColor(110, 110, 120));
		MakeBoxComponent(this, VisualRoot, MoveTemp(M.Data));
		Mesh->SetVisibility(false);
	}
	const TCHAR* Block = Variant == TEXT("chest_minecart") ? TEXT("chest") : (Variant == TEXT("furnace_minecart") ? TEXT("furnace") : (Variant == TEXT("tnt_minecart") ? TEXT("tnt") : (Variant == TEXT("hopper_minecart") ? TEXT("hopper") : nullptr)));
	if (Block) { Content->SetBlockState(FMCBlocks::FindState(Block), 0.75f); Content->SetVisibility(true, true); }
	else Content->Clear();
}

void AMCMinecart::GetRailExits(int32 Shape, FIntVector& A, FIntVector& B) const { GetRailExitsFor(Shape, A, B); }

void AMCMinecart::MoveAlongTrack(const FMCBlockPos& RailPos, FMCState Rail)
{
	const int32 Shape = RailShape(Rail);
	FIntVector A, B;
	GetRailExits(Shape, A, B);
	FVector2D Dir(B.X - A.X, B.Y - A.Y);
	Dir.Normalize();
	const FName RN = FMCBlocks::GetByState(Rail).Name;
	const bool bPowered = RN == TEXT("powered_rail") && (FMCBlocks::MetaOf(Rail) & 8) != 0;
	const bool bBrake = RN == TEXT("powered_rail") && !bPowered;
	// slope pull
	if (Shape >= 2 && Shape <= 5)
	{
		const FIntVector Up = A.Z > 0 ? A : B;
		Vel.X -= Up.X * 0.0078125;
		Vel.Y -= Up.Y * 0.0078125;
	}
	double Along = Vel.X * Dir.X + Vel.Y * Dir.Y;
	// rider push
	if (AMCPlayer* Rider = Passengers.Num() > 0 ? Cast<AMCPlayer>(Passengers[0].Get()) : nullptr)
	{
		if (Rider->Input.Forward > 0.1f && FMath::Abs(Along) < 0.01)
		{
			const FVector L = FRotator(0, Rider->ViewYaw, 0).Vector();
			Along += (L.X * Dir.X + L.Y * Dir.Y) * 0.1;
		}
	}
	if (bBrake)
	{
		if (FMath::Abs(Along) < 0.03) Along = 0.0; else Along *= 0.5;
	}
	if (bPowered)
	{
		if (FMath::Abs(Along) > 0.01) Along += FMath::Sign(Along) * 0.06;
		else
		{
			// kick start away from a solid block
			const FMCBlockPos PA = RailPos + FIntVector(A.X, A.Y, 0), PB = RailPos + FIntVector(B.X, B.Y, 0);
			if (FMCBlocks::IsOpaque(World->GetState(PA))) Along = 0.02;
			else if (FMCBlocks::IsOpaque(World->GetState(PB))) Along = -0.02;
		}
	}
	// furnace minecart propulsion
	if (Variant == TEXT("furnace_minecart") && Fuel > 0)
	{
		--Fuel;
		const double PushAlong = Push.X * Dir.X + Push.Y * Dir.Y;
		if (FMath::Abs(PushAlong) > 1e-4) Along += FMath::Sign(PushAlong) * 0.02;
		if (Rand().NextInt(4) == 0) World->SpawnParticles(TEXT("smoke_large"), Pos + FVector(0, 0, 0.8), 1, 0.1f);
	}
	const double MaxSpeed = bInWater ? 0.2 : 0.4;
	Along = FMath::Clamp(Along, -MaxSpeed, MaxSpeed);
	Vel.X = Dir.X * Along;
	Vel.Y = Dir.Y * Along;
	// snap perpendicular to the rail centre and follow the track height
	FVector NewPos = Pos + FVector(Vel.X, Vel.Y, 0);
	const double CX = RailPos.X + 0.5, CY = RailPos.Y + 0.5;
	if (Shape == 0 || Shape == 4 || Shape == 5) NewPos.X = CX;
	else if (Shape == 1 || Shape == 2 || Shape == 3) NewPos.Y = CY;
	double Z = RailPos.Z + 0.0625;
	if (Shape >= 2 && Shape <= 5)
	{
		const FIntVector Up = A.Z > 0 ? A : B;
		const double T = Up.X != 0 ? (NewPos.X - RailPos.X) * Up.X + (Up.X < 0 ? 1.0 : 0.0) : (NewPos.Y - RailPos.Y) * Up.Y + (Up.Y < 0 ? 1.0 : 0.0);
		Z += FMath::Clamp(T, 0.0, 1.0);
	}
	NewPos.Z = Z;
	// stop at solid blocks in the way
	if (!World->IsRegionFree(GetBoxAt(NewPos).Inflate(-0.1).Offset(0, 0, 0.1)))
	{
		Vel.X = Vel.Y = 0.0;
		NewPos = FVector(Pos.X, Pos.Y, Z);
	}
	Pos = NewPos;
	Vel.Z = 0.0;
	bOnGround = true;
	bOnRail = true;
	// activator rails
	if (RN == TEXT("activator_rail") && (FMCBlocks::MetaOf(Rail) & 8) != 0)
	{
		if (Variant == TEXT("tnt_minecart") && TNTFuse < 0) { TNTFuse = 80; PlaySound(TEXT("tnt_primed"), 1.f, 1.f); }
		if (Passengers.Num() > 0 && Passengers[0].IsValid() && !Passengers[0]->IsA<AMCPlayer>()) Passengers[0]->StopRiding();
	}
	if (FMath::Abs(Along) > 0.05 && Age % 12 == 0) PlaySound(TEXT("minecart_rolling"), (float)FMath::Min(1.0, FMath::Abs(Along) * 2.0), 1.f);
}

void AMCMinecart::TickEntity()
{
	Super::TickEntity();
	if (bRemoved || !World) return;
	if (HurtTicks > 0) --HurtTicks;
	if (Damage > 0.f) Damage = FMath::Max(0.f, Damage - 1.f);
	bOnRail = false;
	FMCBlockPos RP(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z));
	FMCState RS = World->GetState(RP);
	if (!IsRail(RS)) { RP = RP.Down(); RS = World->GetState(RP); }
	if (IsRail(RS))
	{
		const double Friction = Passengers.Num() > 0 ? 0.997 : 0.96;
		MoveAlongTrack(RP, RS);
		Vel.X *= Friction; Vel.Y *= Friction;
		// detector rails notice us through OnEntityInside
		UpdateInsideBlocks();
	}
	else
	{
		Vel.Z -= Gravity;
		Move(Vel);
		if (bOnGround) { Vel.X *= 0.5; Vel.Y *= 0.5; }
		Vel *= 0.95;
	}
	// push other carts / entities
	TArray<AMCEntity*> Near;
	World->GetEntitiesInBox(GetBox().Inflate(0.2), Near, this);
	for (AMCEntity* E : Near)
	{
		if (E == Vehicle.Get() || Passengers.Contains(E)) continue;
		if (AMCMinecart* O = Cast<AMCMinecart>(E))
		{
			const FVector D = (O->Pos - Pos);
			if (D.SizeSquared() < 1e-4) continue;
			const FVector Pushv = D.GetSafeNormal2D() * 0.05;
			O->Vel += Pushv; Vel -= Pushv;
		}
		else if (AMCMob* M = Cast<AMCMob>(E))
		{
			// empty carts pick up mobs
			if (Variant == TEXT("minecart") && Passengers.Num() == 0 && !M->IsPassenger() && M->Def && !M->Def->bBoss && M->Width < 1.1f && FVector2D(Vel.X, Vel.Y).Size() > 0.01) M->StartRiding(this);
		}
	}
	// hopper cart: pick up items above
	if (Variant == TEXT("hopper_minecart") && Age % 4 == 0)
	{
		TArray<AMCEntity*> Items;
		World->GetEntitiesInBox(GetBox().Offset(0, 0, 0.5).Inflate(0.25), Items, this);
		for (AMCEntity* E : Items)
		{
			AMCItemEntity* IE = Cast<AMCItemEntity>(E);
			if (!IE || IE->bRemoved) continue;
			FMCItemStack S = IE->Stack;
			Chest.Insert(S);
			if (S.IsEmpty()) IE->Discard(); else IE->Stack = S;
		}
	}
	// tnt cart
	if (TNTFuse > 0)
	{
		World->SpawnParticles(TEXT("smoke"), Pos + FVector(0, 0, 1.0), 1, 0.1f);
		if (--TNTFuse == 0)
		{
			const double Speed = FMath::Min(5.0, Vel.Size() * 20.0);
			Discard();
			World->Explode(Pos + FVector(0, 0, 0.5), 4.f + Rand().NextFloat() * 1.5f * (float)Speed, false, true, this);
			return;
		}
	}
	// orientation follows the velocity
	if (FVector2D(Vel.X, Vel.Y).SizeSquared() > 1e-5)
	{
		const float Target = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X));
		float D = FMath::FindDeltaAngleDegrees(Yaw, Target);
		if (FMath::Abs(D) > 90.f) D = FMath::FindDeltaAngleDegrees(Yaw, Target + 180.f);
		Yaw += D;
	}
	for (TWeakObjectPtr<AMCEntity>& P : Passengers)
	{
		if (AMCEntity* E = P.Get()) { E->PrevPos = E->Pos; E->Pos = Pos + GetPassengerOffset(E); E->FallDistance = 0.f; }
	}
}

void AMCMinecart::UpdateVisual(float Alpha, float DeltaSeconds)
{
	const FVector P = InterpolatedPos(Alpha);
	SetActorLocation(P * MC::BlockSize);
	const float Y = FMath::Lerp(PrevYaw, PrevYaw + FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Alpha);
	float Pt = 0.f;
	if (World)
	{
		const FMCBlockPos RP(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z));
		const FMCState RS = World->GetState(RP);
		if (IsRail(RS))
		{
			const int32 Shape = RailShape(RS);
			if (Shape >= 2 && Shape <= 5) Pt = 45.f;
		}
	}
	const float Roll = HurtTicks > 0 ? FMath::Sin((HurtTicks - Alpha) * 1.2f) * (HurtTicks - Alpha) * Damage / 10.f : 0.f;
	SetActorRotation(FRotator(Pt, Y, Roll));
	if (TNTFuse > 0) Content->SetFlash(TNTFuse / 5 % 2 == 0 ? 0.8f : 0.f);
}

bool AMCMinecart::Hurt(const FMCDamage& D, float Amount)
{
	if (bRemoved || !World) return false;
	const AMCPlayer* P = Cast<AMCPlayer>(D.Attacker.Get());
	HurtTicks = 10;
	Damage += Amount * 10.f;
	if (Variant == TEXT("tnt_minecart") && (D.bFire || D.bExplosion)) { TNTFuse = FMath::Max(1, FMath::Min(TNTFuse < 0 ? 999 : TNTFuse, D.bExplosion ? 1 : 80)); return true; }
	if ((P && P->IsCreative()) || Damage > 40.f)
	{
		if (!(P && P->IsCreative()) && (!Game || Game->Rules.bDoTileDrops))
		{
			World->SpawnItem(Pos + FVector(0, 0, 0.5), FMCItemStack::Of(Variant, 1));
			for (FMCItemStack& S : Chest.Slots) if (!S.IsEmpty()) { World->SpawnItem(Pos + FVector(0, 0, 0.5), S); S.Clear(); }
		}
		for (TWeakObjectPtr<AMCEntity>& Ps : Passengers) if (AMCEntity* E = Ps.Get()) E->Vehicle.Reset();
		Passengers.Reset();
		Discard();
	}
	return true;
}

bool AMCMinecart::Interact(AMCPlayer* Player, bool bOffHand)
{
	if (!Player || bOffHand) return false;
	if (Variant == TEXT("chest_minecart"))
	{
		Player->OpenMenu(MakeShared<FMCChestMenu>(Player, EMCMenuType::Chest, &Chest, nullptr, 3, FString(TEXT("Minecart with Chest"))));
		return true;
	}
	if (Variant == TEXT("hopper_minecart"))
	{
		Player->OpenMenu(MakeShared<FMCChestMenu>(Player, EMCMenuType::Hopper, &Chest, nullptr, 1, FString(TEXT("Minecart with Hopper"))));
		return true;
	}
	if (Variant == TEXT("furnace_minecart"))
	{
		FMCItemStack& H = Player->Held();
		if (!H.IsEmpty() && (H.Item().Name == TEXT("coal") || H.Item().Name == TEXT("charcoal")))
		{
			Fuel += 3600;
			if (!Player->IsCreative()) { H.Count -= 1; if (H.Count <= 0) H.Clear(); }
		}
		Push = Pos - Player->Pos; Push.Z = 0;
		return true;
	}
	if (Variant == TEXT("minecart") && Passengers.Num() == 0 && !Player->IsPassenger() && !Player->bSneaking)
	{
		Player->StartRiding(this);
		return true;
	}
	return false;
}

void AMCMinecart::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Variant << Fuel << Push << TNTFuse;
	Chest.Serialize(Ar);
	if (Ar.IsLoading()) InitEntity();
}
