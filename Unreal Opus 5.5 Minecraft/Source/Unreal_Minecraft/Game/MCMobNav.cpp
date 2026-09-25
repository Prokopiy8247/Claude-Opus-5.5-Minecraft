// Mob navigation: grid A* pathfinding, steering, random wander targets, target queries, ranged / melee helpers.
#include "Game/MCMob.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"

namespace
{
	/** Global path search budget per game tick (keeps big mob counts cheap). */
	int64 GPathTick = -1;
	int32 GPathBudget = 0;

	bool ConsumePathBudget(const FMCWorld& W)
	{
		if (GPathTick != W.GameTick) { GPathTick = W.GameTick; GPathBudget = 6; }
		if (GPathBudget <= 0) return false;
		--GPathBudget;
		return true;
	}

	bool IsDangerous(FMCState S)
	{
		if (S == 0) return false;
		const FMCStateInfo& I = FMCBlocks::Info(S);
		if (I.Block == FMCBlocks::C.LavaId || I.Block == FMCBlocks::C.FireId) return true;
		const FName N = FMCBlocks::Get(I.Block).Name;
		return N == TEXT("soul_fire") || N == TEXT("cactus") || N == TEXT("sweet_berry_bush") || N == TEXT("magma_block") || N == TEXT("powder_snow")
			|| N == TEXT("cobweb") || N == TEXT("wither_rose") || N == TEXT("campfire") || N == TEXT("soul_campfire") || N == TEXT("pointed_dripstone");
	}

	/** Can an entity occupy this cell (no collision, not harmful)? */
	bool Passable(const FMCWorld& W, const FMCBlockPos& P, bool bCanSwim)
	{
		const FMCState S = W.GetState(P);
		if (S == 0) return true;
		const FMCStateInfo& I = FMCBlocks::Info(S);
		if (IsDangerous(S)) return false;
		if (I.Flags & MCB_Fluid) return bCanSwim || I.Block == FMCBlocks::C.WaterId;
		if (I.bNoCollision) return true;
		// doors / gates count as obstacles; low partial blocks (carpets, snow layers) are walkable
		if (I.bFullCollision) return false;
		TArray<FMCBox> Boxes;
		FMCBlocks::GetCollision(S, &W, P, Boxes);
		double Top = 0.0;
		for (const FMCBox& B : Boxes) Top = FMath::Max(Top, B.Max.Z);
		return Top <= 0.2;
	}

	/** Solid support at the top of cell P (standing surface for P.Up()). */
	bool Supports(const FMCWorld& W, const FMCBlockPos& P, bool bCanSwim)
	{
		const FMCState S = W.GetState(P);
		if (S == 0) return false;
		const FMCStateInfo& I = FMCBlocks::Info(S);
		if (I.Flags & MCB_Fluid) return bCanSwim;
		if (I.bNoCollision) return false;
		if (IsDangerous(S) && FMCBlocks::Get(I.Block).Name != TEXT("pointed_dripstone")) return false;
		if (I.bFullCollision) return true;
		TArray<FMCBox> Boxes;
		FMCBlocks::GetCollision(S, &W, P, Boxes);
		double Top = 0.0;
		for (const FMCBox& B : Boxes) Top = FMath::Max(Top, B.Max.Z);
		return Top > 0.3 && Top <= 1.01; // fences and walls (1.5 high) cannot be stood on
	}

	struct FNode
	{
		FMCBlockPos P;
		float G = 0.f, F = 0.f;
		int32 Parent = -1;
	};
}

bool FMCPathfinder::IsWalkable(const FMCWorld& W, const FMCBlockPos& P, int32 HeightBlocks, bool bCanSwim)
{
	if (!FMCChunk::InRange(P.Z) || !W.IsReadyAt(P)) return false;
	for (int32 h = 0; h < HeightBlocks; ++h) if (!Passable(W, P.Up(h), bCanSwim)) return false;
	if (bCanSwim && FMCBlocks::IsFluid(W.GetState(P))) return true;
	return Supports(W, P.Down(), bCanSwim) || (FMCBlocks::Info(W.GetState(P)).Flags & MCB_Climbable) != 0;
}

bool FMCPathfinder::FindPath(const FMCWorld& W, const FMCBlockPos& Start, const FMCBlockPos& Goal, int32 MaxNodes, float MobWidth, float MobHeight,
	bool bCanSwim, bool bAvoidWater, TArray<FMCBlockPos>& OutPath, int32 MaxDrop)
{
	OutPath.Reset();
	const int32 H = FMath::Max(1, FMath::CeilToInt(MobHeight));
	TArray<FNode> Nodes;
	Nodes.Reserve(MaxNodes + 16);
	TMap<FMCBlockPos, int32> Index;
	TArray<int32> Open;
	auto Heur = [&](const FMCBlockPos& P) { return FMath::Sqrt((float)P.DistSq(Goal)); };
	FNode S0; S0.P = Start; S0.G = 0.f; S0.F = Heur(Start);
	Nodes.Add(S0);
	Index.Add(Start, 0);
	Open.Add(0);
	TSet<int32> Closed;
	int32 Best = 0;
	float BestH = S0.F;
	static const FIntVector Dirs[8] = { FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0),
		FIntVector(1, 1, 0), FIntVector(1, -1, 0), FIntVector(-1, 1, 0), FIntVector(-1, -1, 0) };
	int32 Found = -1;
	while (Open.Num() > 0 && Nodes.Num() < MaxNodes)
	{
		// pop lowest F (small open lists: linear scan is fine)
		int32 BI = 0;
		for (int32 i = 1; i < Open.Num(); ++i) if (Nodes[Open[i]].F < Nodes[Open[BI]].F) BI = i;
		const int32 Cur = Open[BI];
		Open.RemoveAtSwap(BI, EAllowShrinking::No);
		if (Closed.Contains(Cur)) continue;
		Closed.Add(Cur);
		const FNode CN = Nodes[Cur];
		const float Hc = Heur(CN.P);
		if (Hc < BestH) { BestH = Hc; Best = Cur; }
		if (CN.P == Goal || (FMath::Abs(CN.P.X - Goal.X) <= 0 && FMath::Abs(CN.P.Y - Goal.Y) <= 0 && FMath::Abs(CN.P.Z - Goal.Z) <= 1)) { Found = Cur; break; }
		for (int32 d = 0; d < 8; ++d)
		{
			const FIntVector& D = Dirs[d];
			const bool bDiag = d >= 4;
			if (bDiag)
			{
				// no corner cutting
				if (!IsWalkable(W, FMCBlockPos(CN.P.X + D.X, CN.P.Y, CN.P.Z), H, bCanSwim) || !IsWalkable(W, FMCBlockPos(CN.P.X, CN.P.Y + D.Y, CN.P.Z), H, bCanSwim)) continue;
			}
			FMCBlockPos N(CN.P.X + D.X, CN.P.Y + D.Y, CN.P.Z);
			bool bOk = IsWalkable(W, N, H, bCanSwim);
			float Extra = 0.f;
			if (!bOk && !bDiag)
			{
				// step up one block (needs head room above the current node)
				const FMCBlockPos Up = N.Up();
				if (Passable(W, CN.P.Up(H), bCanSwim) && IsWalkable(W, Up, H, bCanSwim)) { N = Up; bOk = true; Extra = 0.5f; }
			}
			if (!bOk)
			{
				// drop down
				for (int32 Drop = 1; Drop <= MaxDrop && !bOk; ++Drop)
				{
					const FMCBlockPos Dn = N.Down(Drop);
					bool bClear = true;
					for (int32 k = 0; k < Drop && bClear; ++k) bClear = Passable(W, N.Down(k), bCanSwim) && Passable(W, N.Down(k).Up(H - 1), bCanSwim);
					if (!bClear) break;
					if (IsWalkable(W, Dn, H, bCanSwim)) { N = Dn; bOk = true; Extra = 0.2f * Drop; }
				}
			}
			if (!bOk) continue;
			if (bAvoidWater && FMCBlocks::IsFluid(W.GetState(N))) Extra += 4.f;
			const float G = CN.G + (bDiag ? 1.4142f : 1.f) + Extra;
			if (const int32* Ex = Index.Find(N))
			{
				if (G < Nodes[*Ex].G && !Closed.Contains(*Ex))
				{
					Nodes[*Ex].G = G; Nodes[*Ex].F = G + Heur(N); Nodes[*Ex].Parent = Cur;
					Open.Add(*Ex);
				}
				continue;
			}
			FNode NN; NN.P = N; NN.G = G; NN.F = G + Heur(N); NN.Parent = Cur;
			const int32 NI = Nodes.Add(NN);
			Index.Add(N, NI);
			Open.Add(NI);
		}
	}
	const int32 End = Found >= 0 ? Found : Best;
	if (End == 0) return false;
	for (int32 i = End; i > 0; i = Nodes[i].Parent) OutPath.Insert(Nodes[i].P, 0);
	return Found >= 0 || OutPath.Num() > 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Steering

void AMCMob::FaceTowards(const FVector& P, float MaxTurn)
{
	const FVector D = P - Pos;
	if (FMath::Abs(D.X) + FMath::Abs(D.Y) < 1e-4) return;
	const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, WantYaw), -MaxTurn, MaxTurn);
}

void AMCMob::MoveTowards(const FVector& Goal, float SpeedMul)
{
	FaceTowards(Goal, 30.f);
	const FVector D = Goal - Pos;
	const double H = FMath::Sqrt(D.X * D.X + D.Y * D.Y);
	if (H < 0.1) { MoveForward = 0.f; return; }
	const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))));
	MoveForward = Delta > 60.f ? 0.25f : 1.f;
	SpeedModifier = SpeedMul;
	// jump up ledges / out of water
	if ((bHorizontalCollision && bOnGround && D.Z > -0.5) || (bInWater && D.Z > 0.2) || (bInWater && bHorizontalCollision)) bJumping = true;
}

bool AMCMob::NavigateTo(const FVector& Goal, float SpeedMul)
{
	if (!World) return false;
	const FMCBlockPos G(MC::FloorToInt(Goal.X), MC::FloorToInt(Goal.Y), MC::FloorToInt(Goal.Z));
	const bool bNeedRepath = !HasPath() || G.DistSq(PathGoal) > 2 || RepathTimer <= 0;
	if (--RepathTimer < 0) RepathTimer = 0;
	if (bNeedRepath && (World->GameTick - LastPathTick > 10 || !HasPath()))
	{
		if (ConsumePathBudget(*World))
		{
			LastPathTick = (int32)World->GameTick;
			Path.Reset(); PathIndex = 0;
			const float Range = FMath::Max(16.f, Def ? Def->FollowRange : 16.f);
			const int32 MaxNodes = FMath::Clamp((int32)(Range * 12.f), 120, 480);
			const bool bSwim = (Def && (Def->bAmphibious || Def->bSwimmer)) || bInWater;
			FMCPathfinder::FindPath(*World, BlockPos(), G, MaxNodes, Width, Height, bSwim, !bSwim, Path, 3);
			PathGoal = G;
			RepathTimer = 40 + Rand().NextInt(20);
		}
	}
	if (!HasPath())
	{
		// open terrain fallback: walk straight
		if (FVector::DistSquared(Pos, Goal) < 1024.0) MoveTowards(Goal, SpeedMul);
		return false;
	}
	TickFollowPath(SpeedMul);
	return true;
}

void AMCMob::TickFollowPath(float SpeedMul)
{
	while (HasPath())
	{
		const FMCBlockPos& N = Path[PathIndex];
		const FVector C(N.X + 0.5, N.Y + 0.5, N.Z);
		const double DX = C.X - Pos.X, DY = C.Y - Pos.Y;
		const double Reach = FMath::Max(0.35, Width * 0.5);
		if (DX * DX + DY * DY < Reach * Reach && FMath::Abs(C.Z - Pos.Z) < 1.2) { ++PathIndex; continue; }
		MoveTowards(C, SpeedMul);
		if (C.Z > Pos.Z + 0.5 && bOnGround && (bHorizontalCollision || DX * DX + DY * DY < 1.5)) bJumping = true;
		return;
	}
	MoveForward = 0.f;
}

void AMCMob::TickWander(float SpeedMul, int32 Chance)
{
	if (!bHasWander)
	{
		if (Rand().NextInt(FMath::Max(1, Chance)) != 0) { if (!HasPath()) MoveForward = 0.f; else TickFollowPath(SpeedMul); return; }
		FVector T;
		if (FindRandomWanderTarget(10, 7, T, Def && (Def->bSwimmer))) { WanderTarget = T; bHasWander = true; StopNavigation(); }
		return;
	}
	if (FVector::DistSquared(FVector(Pos.X, Pos.Y, 0), FVector(WanderTarget.X, WanderTarget.Y, 0)) < 1.0 || NoActionTime > 200)
	{
		bHasWander = false;
		StopNavigation();
		MoveForward = 0.f;
		return;
	}
	NavigateTo(WanderTarget, SpeedMul);
}

bool AMCMob::FindRandomWanderTarget(int32 Radius, int32 VRadius, FVector& Out, bool bPreferWater) const
{
	if (!World) return false;
	FVector BestP;
	float BestScore = -1e9f;
	const bool bSwim = Def && (Def->bSwimmer || Def->bAmphibious);
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const int32 X = MC::FloorToInt(Pos.X) + Rand().Range(-Radius, Radius);
		const int32 Y = MC::FloorToInt(Pos.Y) + Rand().Range(-Radius, Radius);
		int32 Z = MC::FloorToInt(Pos.Z) + Rand().Range(-VRadius, VRadius);
		const FMCBlockPos Probe(X, Y, Z);
		if (!World->IsReadyAt(Probe)) continue;
		// settle onto the ground below
		int32 Guard = 0;
		while (Guard++ < VRadius * 2 && Z > MC::MinZ && !FMCPathfinder::IsWalkable(*World, FMCBlockPos(X, Y, Z), FMath::CeilToInt(Height), bSwim)) --Z;
		const FMCBlockPos P(X, Y, Z);
		if (!FMCPathfinder::IsWalkable(*World, P, FMath::CeilToInt(Height), bSwim)) continue;
		float Score = 0.f;
		const FName Ground = FMCBlocks::GetByState(World->GetState(P.Down())).Name;
		if (Def && Def->Category == EMCMobCategory::Creature && Ground == TEXT("grass_block")) Score += 10.f;
		if (Def && Def->Category == EMCMobCategory::Monster) Score += 15.f - World->GetLight(P, Game ? Game->GetSkyDarken() : 0);
		if (bPreferWater && FMCBlocks::IsFluid(World->GetState(P))) Score += 20.f;
		if (bHasHome && Def && Def->AI == EMCMobAI::Villager) Score -= (float)FMath::Sqrt((double)P.DistSq(HomePos)) * 0.5f;
		Score += Rand().NextFloat();
		if (Score > BestScore) { BestScore = Score; BestP = FVector(X + 0.5, Y + 0.5, Z); }
	}
	if (BestScore <= -1e8f) return false;
	Out = BestP;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Flying / swimming movement (no pathfinding; 3D steering)

void AMCMob::TickFlyMovement(float Speed)
{
	FVector D = FlyTarget - Pos;
	const double L = D.Size();
	MoveForward = 0.f;
	if (L > 0.05)
	{
		D /= L;
		Vel += (D * Speed - Vel) * 0.1;
		const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, WantYaw), -10.f, 10.f);
	}
	// obstacle avoidance: pick a new target when blocked
	if (bHorizontalCollision || bVerticalCollision) FlyTarget = Pos + FVector(Rand().FRange(-6.f, 6.f), Rand().FRange(-6.f, 6.f), Rand().FRange(-2.f, 4.f));
}

void AMCMob::TickSwimMovement(float Speed)
{
	MoveForward = 0.f;
	if (!bInWater)
	{
		// out of water: flop around
		if (bOnGround && Rand().NextInt(10) == 0 && Def && Def->bSwimmer)
		{
			Vel += FVector(Rand().FRange(-0.1f, 0.1f), Rand().FRange(-0.1f, 0.1f), 0.4);
			PlaySound(TEXT("fish_flop"), 1.f, 1.f);
		}
		return;
	}
	FVector D = FlyTarget - Pos;
	const double L = D.Size();
	if (L < 0.8 || Rand().NextInt(80) == 0)
	{
		// new random target inside water
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const FVector T = Pos + FVector(Rand().FRange(-8.f, 8.f), Rand().FRange(-8.f, 8.f), Rand().FRange(-3.f, 3.f));
			const FMCState S = World->GetState(FMCBlockPos(MC::FloorToInt(T.X), MC::FloorToInt(T.Y), MC::FloorToInt(T.Z)));
			if (FMCBlocks::Info(S).Block == FMCBlocks::C.WaterId || (FMCBlocks::Info(S).Flags & MCB_Waterlogged)) { FlyTarget = T; break; }
		}
		return;
	}
	D /= L;
	Vel += (D * Speed * 0.12 - Vel) * 0.08;
	const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, WantYaw), -12.f, 12.f);
	Pitch = FMath::Lerp(Pitch, (float)FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(D.Z, -1.0, 1.0))), 0.1f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

AMCPlayer* AMCMob::FindNearestPlayer(float Range, bool bNeedSight) const
{
	if (!Game || !Game->Player || !World) return nullptr;
	AMCPlayer* P = Game->Player;
	if (P->World != World || !P->IsAlive() || P->IsSpectator()) return nullptr;
	float R = Range;
	// sneaking / invisibility / mob heads reduce detection range
	if (P->bSneaking) R *= 0.8f;
	if (P->HasEffect(EMCEffect::Invisibility)) R *= 0.07f;
	const FMCItemStack& Head = P->Inventory.Slots[39];
	if (!Head.IsEmpty() && Def)
	{
		const FString HN = Head.Item().Name.ToString();
		if ((Def->Id == TEXT("zombie") && HN == TEXT("zombie_head")) || (Def->Id == TEXT("skeleton") && HN == TEXT("skeleton_skull")) || (Def->Id == TEXT("creeper") && HN == TEXT("creeper_head"))) R *= 0.5f;
	}
	if (FVector::DistSquared(P->Pos, Pos) > R * R) return nullptr;
	if (bNeedSight && !CanSee(P)) return nullptr;
	return P;
}

AMCLiving* AMCMob::FindNearestMob(float Range, TFunctionRef<bool(AMCLiving*)> Pred) const
{
	if (!World) return nullptr;
	TArray<AMCEntity*> Near;
	World->GetEntitiesInBox(GetBox().Inflate(Range), Near, this);
	AMCLiving* Best = nullptr;
	double BestD = Range * Range;
	for (AMCEntity* E : Near)
	{
		AMCLiving* L = Cast<AMCLiving>(E);
		if (!L || !L->IsAlive() || !Pred(L)) continue;
		const double D = FVector::DistSquared(L->Pos, Pos);
		if (D < BestD) { BestD = D; Best = L; }
	}
	return Best;
}

bool AMCMob::IsSunBurnTick() const
{
	if (!Game || !World || World->Dim != EMCDimension::Overworld || !Game->IsDay()) return false;
	if (bInWater || IsOnFire() || Game->IsRainingAt(BlockPos())) return false;
	if (!GetItem(EMCEquipSlot::Head).IsEmpty()) return false;
	const FMCBlockPos Eye(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z + EyeHeight));
	if (!World->CanSeeSky(Eye)) return false;
	const float B = GetBrightness();
	return B > 0.5f && Rand().NextFloat() * 30.f < (B - 0.4f) * 2.f;
}

float AMCMob::AttackReach(const AMCLiving* Victim) const
{
	const float W = Width * 2.f;
	return W * W + (Victim ? Victim->Width : 0.6f);
}

bool AMCMob::DoMeleeAttack(AMCLiving* Victim)
{
	if (!Victim || !Def || AttackCooldown > 0) return false;
	const double DSq = FVector::DistSquared(Pos, Victim->Pos);
	if (DSq > AttackReach(Victim) + 0.5 || FMath::Abs(Victim->Pos.Z - Pos.Z) > 2.5) return false;
	AttackCooldown = 20;
	Swing();
	float Dmg = Def->AttackDamage;
	const FMCItemStack& W = MainHandConst();
	if (!W.IsEmpty() && W.Item().AttackDamage > 1.f) Dmg += W.Item().AttackDamage - 1.f;
	const int32 Str = EffectAmp(EMCEffect::Strength);
	if (Str >= 0) Dmg += 3.f * (Str + 1);
	FMCDamage D = FMCDamage::Mob(this);
	const bool bHit = Victim->Hurt(D, Dmg);
	if (bHit)
	{
		// mob specific hit effects
		const FName Id = Def->Id;
		if (Id == TEXT("husk")) { FMCEffectInstance E; E.Effect = EMCEffect::Hunger; E.Duration = 140; Victim->AddEffect(E); }
		else if (Id == TEXT("cave_spider") && Game && Game->Difficulty >= EMCDifficulty::Normal) { FMCEffectInstance E; E.Effect = EMCEffect::Poison; E.Duration = Game->Difficulty == EMCDifficulty::Hard ? 300 : 140; Victim->AddEffect(E); }
		else if (Id == TEXT("wither_skeleton")) { FMCEffectInstance E; E.Effect = EMCEffect::Wither; E.Duration = 200; Victim->AddEffect(E); }
		else if (Id == TEXT("bee") && Game && Game->Difficulty >= EMCDifficulty::Normal) { FMCEffectInstance E; E.Effect = EMCEffect::Poison; E.Duration = Game->Difficulty == EMCDifficulty::Hard ? 360 : 200; Victim->AddEffect(E); }
		else if (Id == TEXT("iron_golem")) { Victim->Vel.Z += 0.4; PlaySound(TEXT("iron_golem_attack"), 1.f, 1.f); }
		else if (Id == TEXT("ravager") || Id == TEXT("hoglin") || Id == TEXT("zoglin")) { Victim->Vel.Z += 0.3; }
		else if (Id == TEXT("zombie") || Id == TEXT("drowned") || Id == TEXT("zombie_villager"))
		{
			if (IsOnFire() && Rand().NextFloat() < 0.3f * (Game ? (float)Game->Difficulty : 2.f)) Victim->SetOnFire(2 * (Game ? (int32)Game->Difficulty : 2));
		}
		const int32 FA = W.IsEmpty() ? 0 : W.GetEnchant(EMCEnchant::FireAspect);
		if (FA > 0) Victim->SetOnFire(4 * FA);
	}
	return bHit;
}

void AMCMob::ShootProjectile(FName TypeName, AMCLiving* At, float Speed, float Inaccuracy)
{
	if (!Game || !World || !At) return;
	const FVector From = GetEyePos() - FVector(0, 0, 0.1);
	AMCProjectile* P = Game->SpawnEntity<AMCProjectile>(World, From);
	if (!P) return;
	P->Shooter = this;
	P->bPickup = false;
	const FString T = TypeName.ToString();
	if (T == TEXT("arrow")) { P->Type = EMCProjectile::Arrow; P->Damage = 2.f + (Game->Difficulty == EMCDifficulty::Hard ? 1.f : 0.f); P->Item = FMCItemStack::Of(TEXT("arrow"), 1); }
	else if (T == TEXT("slow_arrow")) { P->Type = EMCProjectile::Arrow; P->Damage = 2.f; P->Item = FMCItemStack::Of(TEXT("tipped_arrow"), 1); P->Item.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(TEXT("slowness"))); }
	else if (T == TEXT("poison_arrow")) { P->Type = EMCProjectile::Arrow; P->Damage = 2.f; P->Item = FMCItemStack::Of(TEXT("tipped_arrow"), 1); P->Item.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(TEXT("poison"))); }
	else if (T == TEXT("weakness_arrow")) { P->Type = EMCProjectile::Arrow; P->Damage = 2.f; P->Item = FMCItemStack::Of(TEXT("tipped_arrow"), 1); P->Item.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(TEXT("weakness"))); }
	else if (T == TEXT("fireball")) { P->Type = EMCProjectile::Fireball; P->Damage = 6.f; }
	else if (T == TEXT("small_fireball")) { P->Type = EMCProjectile::SmallFireball; P->Damage = 5.f; }
	else if (T == TEXT("wither_skull")) { P->Type = EMCProjectile::WitherSkull; P->Damage = 8.f; }
	else if (T == TEXT("blue_wither_skull")) { P->Type = EMCProjectile::WitherSkull; P->Damage = 8.f; P->bDangerous = true; }
	else if (T == TEXT("shulker_bullet")) { P->Type = EMCProjectile::ShulkerBullet; P->Damage = 4.f; P->HomingTarget = At; }
	else if (T == TEXT("llama_spit")) { P->Type = EMCProjectile::LlamaSpit; P->Damage = 1.f; }
	else if (T == TEXT("snowball")) { P->Type = EMCProjectile::Snowball; P->Damage = 0.f; P->Item = FMCItemStack::Of(TEXT("snowball"), 1); }
	else if (T == TEXT("wind_charge")) { P->Type = EMCProjectile::BreezeWindCharge; P->Damage = 1.f; }
	else if (T == TEXT("trident")) { P->Type = EMCProjectile::Trident; P->Damage = 8.f; P->Item = FMCItemStack::Of(TEXT("trident"), 1); }
	else if (T == TEXT("potion"))
	{
		P->Type = EMCProjectile::Potion;
		P->Item = FMCItemStack::Of(TEXT("splash_potion"), 1);
		const TCHAR* Choice = At->HasEffect(EMCEffect::Slowness) ? (At->Health >= 8.f && !At->HasEffect(EMCEffect::Poison) ? TEXT("poison") : TEXT("harming")) : TEXT("slowness");
		if (At->IsA<AMCMob>() && Cast<AMCMob>(At)->bUndead) Choice = TEXT("healing");
		P->Item.MutableExtra().Potion = (uint8)FMath::Max(0, MCPotions::Find(Choice));
	}
	P->InitEntity();
	const FVector AimAt = At->Pos + FVector(0, 0, At->Height * 0.35);
	FVector Dir = AimAt - From;
	const double Dist = FVector(Dir.X, Dir.Y, 0).Size();
	// arc compensation for gravity-affected projectiles
	if (P->Type == EMCProjectile::Arrow || P->Type == EMCProjectile::Snowball || P->Type == EMCProjectile::Potion || P->Type == EMCProjectile::Trident || P->Type == EMCProjectile::LlamaSpit)
		Dir.Z += Dist * 0.2;
	P->Shoot(Dir.GetSafeNormal(), Speed, Inaccuracy * (Game ? (14 - 4 * (int32)Game->Difficulty) : 10) / 10.f);
	if (P->Type == EMCProjectile::Fireball || P->Type == EMCProjectile::SmallFireball || P->Type == EMCProjectile::WitherSkull || P->Type == EMCProjectile::BreezeWindCharge)
	{
		P->Accel = Dir.GetSafeNormal() * 0.1;
	}
}
