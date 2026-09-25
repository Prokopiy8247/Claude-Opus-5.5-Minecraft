// Dimension travel (nether portals with linking, end portals, gateways) and the End dragon fight manager.
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"
#include "Gen/MCEndGen.h"
#include "Render/MCVoxelRenderer.h"
#include "Render/MCParticles.h"
#include "Audio/MCAudio.h"

namespace
{
	FMCState PortalState(bool bAxisY)
	{
		const FMCBlock* B = FMCBlocks::Find(TEXT("nether_portal"));
		return B ? B->State(bAxisY ? 1 : 0) : 0;
	}
	bool IsObsidian(FMCState S) { return S == FMCBlocks::C.Obsidian; }
	bool IsPortalAir(FMCState S) { return S == 0 || FMCBlocks::Is(S, FMCBlocks::C.FireId) || FMCBlocks::GetByState(S).Name == TEXT("soul_fire") || FMCBlocks::GetByState(S).Name == TEXT("nether_portal"); }
}

// ---------------------------------------------------------------------------------------------------------------------
// Nether portal frames

bool AMCGame::TryCreateNetherPortal(FMCWorld& W, const FMCBlockPos& FirePos)
{
	if (W.Dim == EMCDimension::End) return false;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FIntVector Dir = Axis == 0 ? FIntVector(1, 0, 0) : FIntVector(0, 1, 0);
		// bottom of the interior
		FMCBlockPos P = FirePos;
		int32 Guard = 0;
		while (IsPortalAir(W.GetState(P.Down())) && Guard++ < 21) P = P.Down();
		if (!IsObsidian(W.GetState(P.Down()))) continue;
		// left edge
		Guard = 0;
		while (IsPortalAir(W.GetState(P + FIntVector(-Dir.X, -Dir.Y, 0))) && IsObsidian(W.GetState(P.Down() + FIntVector(-Dir.X, -Dir.Y, 0))) && Guard++ < 21) P = P + FIntVector(-Dir.X, -Dir.Y, 0);
		if (!IsObsidian(W.GetState(P + FIntVector(-Dir.X, -Dir.Y, 0)))) continue;
		// width
		int32 Width = 0;
		while (Width < 22 && IsPortalAir(W.GetState(P + Dir * Width)) && IsObsidian(W.GetState((P + Dir * Width).Down()))) ++Width;
		if (Width < 2 || Width > 21 || !IsObsidian(W.GetState(P + Dir * Width))) continue;
		// height
		int32 Height = 0;
		bool bOk = true;
		for (; Height < 22; ++Height)
		{
			bool bRow = true;
			for (int32 x = 0; x < Width; ++x) if (!IsPortalAir(W.GetState((P + Dir * x).Up(Height)))) { bRow = false; break; }
			if (!bRow) break;
			if (!IsObsidian(W.GetState((P + FIntVector(-Dir.X, -Dir.Y, 0)).Up(Height))) || !IsObsidian(W.GetState((P + Dir * Width).Up(Height)))) { bOk = false; break; }
		}
		if (!bOk || Height < 3 || Height > 21) continue;
		for (int32 x = 0; x < Width; ++x) if (!IsObsidian(W.GetState((P + Dir * x).Up(Height)))) { bOk = false; break; }
		if (!bOk) continue;
		// fill
		const FMCState PS = PortalState(Axis == 1);
		for (int32 h = 0; h < Height; ++h)
			for (int32 x = 0; x < Width; ++x)
			{
				const FMCBlockPos C = (P + Dir * x).Up(h);
				W.SetState(C, PS, MCSet_Render | MCSet_Light);
				W.AddPortalPOI(C);
			}
		W.PlaySound(TEXT("portal_ignite"), FVector(FirePos.X + 0.5, FirePos.Y + 0.5, FirePos.Z + 0.5), 1.f, 1.f);
		return true;
	}
	return false;
}

FVector AMCGame::FindOrCreatePortal(FMCWorld* Target, const FVector& FromPos, EMCDimension From)
{
	const double Scale = From == EMCDimension::Overworld ? 1.0 / 8.0 : 8.0;
	FVector S(FromPos.X * Scale, FromPos.Y * Scale, FromPos.Z);
	const bool bToNether = Target->Dim == EMCDimension::Nether;
	S.Z = bToNether ? FMath::Clamp(S.Z, 32.0, 118.0) : FMath::Clamp(S.Z, (double)MC::MinZ + 4, 250.0);
	const FMCBlockPos SB(MC::FloorToInt(S.X), MC::FloorToInt(S.Y), MC::FloorToInt(S.Z));
	Target->ForceLoadArea(FMCChunkPos::FromBlock(SB), 2, 15.0);
	const int32 Radius = bToNether ? 16 : 128;
	// 1) known portal blocks
	double Best = 1e18;
	FMCBlockPos Found;
	bool bFound = false;
	for (int32 i = Target->PortalPOIs.Num() - 1; i >= 0; --i)
	{
		const FMCBlockPos& P = Target->PortalPOIs[i];
		if (FMCBlocks::GetByState(Target->GetState(P)).Name != TEXT("nether_portal")) { if (Target->IsReadyAt(P)) Target->PortalPOIs.RemoveAtSwap(i); continue; }
		if (FMath::Abs(P.X - SB.X) > Radius || FMath::Abs(P.Y - SB.Y) > Radius) continue;
		const double D = P.DistSq(SB);
		if (D < Best) { Best = D; Found = P; bFound = true; }
	}
	// 2) scan loaded terrain around the target (portals saved in chunk files)
	if (!bFound)
	{
		const FMCBlock* Portal = FMCBlocks::Find(TEXT("nether_portal"));
		const int32 ScanR = FMath::Min(Radius, 40);
		for (int32 dx = -ScanR; dx <= ScanR && Portal; ++dx)
			for (int32 dy = -ScanR; dy <= ScanR; ++dy)
			{
				const int32 X = SB.X + dx, Y = SB.Y + dy;
				const FMCChunk* C = Target->GetChunk(FMCChunkPos::FromBlock(X, Y));
				if (!C) continue;
				const int32 ZTop = bToNether ? 127 : FMath::Min(MC::MaxZ, C->GetHeight(X & 15, Y & 15) + 2);
				for (int32 Z = bToNether ? 1 : MC::MinZ + 1; Z <= ZTop; ++Z)
				{
					const FMCState St = C->Get(X & 15, Y & 15, Z);
					if (FMCBlocks::BlockOf(St) != Portal->Id) continue;
					const FMCBlockPos P(X, Y, Z);
					Target->AddPortalPOI(P);
					const double D = P.DistSq(SB);
					if (D < Best) { Best = D; Found = P; bFound = true; }
				}
			}
	}
	if (bFound)
	{
		// stand on the bottom portal block of that column
		FMCBlockPos P = Found;
		while (FMCBlocks::GetByState(Target->GetState(P.Down())).Name == TEXT("nether_portal")) P = P.Down();
		return FVector(P.X + 0.5, P.Y + 0.5, P.Z);
	}

	// 3) build a new portal near the scaled position
	FMCBlockPos Site = SB;
	bool bSite = false;
	for (int32 R = 0; R <= 16 && !bSite; ++R)
	{
		for (int32 dx = -R; dx <= R && !bSite; ++dx)
			for (int32 dy = -R; dy <= R && !bSite; ++dy)
			{
				if (FMath::Max(FMath::Abs(dx), FMath::Abs(dy)) != R) continue;
				const int32 X = SB.X + dx, Y = SB.Y + dy;
				const int32 ZStart = bToNether ? 118 : FMath::Min(250, Target->GetHeight(X, Y) + 1);
				for (int32 Z = ZStart; Z > (bToNether ? 32 : MC::MinZ + 5); --Z)
				{
					bool bOk = FMCBlocks::IsSolid(Target->GetState(FMCBlockPos(X, Y, Z - 1))) && !FMCBlocks::IsFluid(Target->GetState(FMCBlockPos(X, Y, Z - 1)));
					for (int32 w = -1; w <= 2 && bOk; ++w)
						for (int32 h = 0; h < 5 && bOk; ++h)
						{
							const FMCState St = Target->GetState(FMCBlockPos(X + w, Y, Z + h));
							if (St != 0 && !FMCBlocks::IsReplaceable(St)) bOk = false;
						}
					if (bOk) { Site = FMCBlockPos(X, Y, Z); bSite = true; break; }
				}
			}
	}
	if (!bSite) Site = FMCBlockPos(SB.X, SB.Y, bToNether ? 70 : FMath::Max(64, SB.Z));
	// frame 4 x 5 along X with a 2 x 3 interior, plus a small platform
	for (int32 w = -1; w <= 2; ++w)
		for (int32 d = -1; d <= 1; ++d)
		{
			const FMCBlockPos Floor(Site.X + w, Site.Y + d, Site.Z - 1);
			if (!FMCBlocks::IsSolid(Target->GetState(Floor)) || FMCBlocks::IsFluid(Target->GetState(Floor))) Target->SetState(Floor, FMCBlocks::C.Obsidian, MCSet_Default);
			for (int32 h = 0; h < 5; ++h) if (d != 0) { const FMCBlockPos A(Site.X + w, Site.Y + d, Site.Z + h); if (h < 3 && Target->GetState(A) != 0 && !FMCBlocks::IsSolid(Target->GetState(A.Down()))) {} if (h < 3) Target->SetState(A, 0, MCSet_Default); }
		}
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 w = -1; w <= 2; ++w)
			for (int32 h = -1; h <= 3; ++h)
			{
				const FMCBlockPos P(Site.X + w, Site.Y, Site.Z + h);
				const bool bFrame = w == -1 || w == 2 || h == -1 || h == 3;
				if (Pass == 0 && bFrame) Target->SetState(P, FMCBlocks::C.Obsidian, MCSet_Default);
				if (Pass == 1 && !bFrame) { Target->SetState(P, PortalState(false), MCSet_Render | MCSet_Light); Target->AddPortalPOI(P); }
			}
	return FVector(Site.X + 0.5, Site.Y + 0.5, Site.Z);
}

// ---------------------------------------------------------------------------------------------------------------------
// Changing dimension (the player carries the camera; the old dimension is saved and unloaded)

FVector AMCGame::FindEndSpawn()
{
	FMCWorld* End = EnsureWorld(EMCDimension::End);
	const FMCBlockPos P = FMCEndGen::PlatformPos();
	End->ForceLoadArea(FMCChunkPos::FromBlock(P), 2, 15.0);
	// the obsidian platform is rebuilt every time (Minecraft behaviour)
	for (int32 dx = -2; dx <= 2; ++dx)
		for (int32 dy = -2; dy <= 2; ++dy)
		{
			End->SetState(FMCBlockPos(P.X + dx, P.Y + dy, P.Z), FMCBlocks::C.Obsidian, MCSet_Default);
			for (int32 dz = 1; dz <= 3; ++dz) End->SetState(FMCBlockPos(P.X + dx, P.Y + dy, P.Z + dz), 0, MCSet_Default);
		}
	return FVector(P.X + 0.5, P.Y + 0.5, P.Z + 1);
}

void AMCGame::ChangeDimension(AMCEntity* E, EMCDimension To, const FVector* ExactPos)
{
	if (!E || !Player) return;
	if (E != Player)
	{
		// only the player travels between loaded dimensions in this build; other entities stay behind
		E->PortalCooldown = 300;
		return;
	}
	const EMCDimension From = ActiveDim;
	FMCWorld* FromW = ActiveWorld();
	if (!FromW) return;
	bLoading = true;
	LoadingText = To == EMCDimension::Nether ? TEXT("Entering the Nether") : (To == EMCDimension::End ? TEXT("Entering the End") : TEXT("Returning to the Overworld"));
	Player->StopRiding();
	for (TWeakObjectPtr<AMCEntity>& P : Player->Passengers) if (P.IsValid()) P->StopRiding();
	const FVector FromPos = Player->Pos;

	// 1) leave the old world
	FromW->UnregisterEntity(Player);
	FromW->bActive = false;
	FromW->UnloadAll(true); // saves modified chunks, captures their entities
	for (AMCEntity* O : TArray<AMCEntity*>(FromW->Entities)) if (O && O != Player) { O->Destroy(); }
	FromW->Entities.Reset();

	// 2) enter the new one
	ActiveDim = To;
	FMCWorld* ToW = EnsureWorld(To);
	ToW->bActive = true;
	if (Renderer) Renderer->SetWorld(ToW);
	if (Particles) Particles->SetWorld(ToW);
	FVector Target;
	if (ExactPos) Target = *ExactPos;
	else if (To == EMCDimension::End) Target = FindEndSpawn();
	else if (From == EMCDimension::End)
	{
		// leaving the End: respawn point or world spawn
		const FMCBlockPos WS = WorldSpawn;
		ToW->ForceLoadArea(FMCChunkPos::FromBlock(WS), 2, 15.0);
		const FMCBlockPos Safe = ToW->FindSafeSpawn(WS.X, WS.Y);
		Target = FVector(Safe.X + 0.5, Safe.Y + 0.5, Safe.Z);
		if (Player->bHasSpawnPoint && Player->SpawnDim == EMCDimension::Overworld)
		{
			const FMCBlockPos SP = Player->SpawnPoint;
			ToW->ForceLoadArea(FMCChunkPos::FromBlock(SP), 1, 10.0);
			if (FMCBlocks::GetByState(ToW->GetState(SP)).Model == EMCModel::Bed) Target = FVector(SP.X + 0.5, SP.Y + 0.5, SP.Z + 1.0);
		}
	}
	else Target = FindOrCreatePortal(ToW, FromPos, From);
	ToW->ForceLoadArea(FMCChunkPos::FromBlock(MC::FloorToInt(Target.X), MC::FloorToInt(Target.Y)), 2, 15.0);
	ToW->RegisterEntity(Player);
	Player->World = ToW;
	Player->TeleportTo(Target);
	Player->PortalCooldown = 300;
	Player->PortalTime = 0;
	Player->OnDimensionChanged();
	bPendingDimChange = true;
	bLoading = false;
	TickAccumulator = 0.0;
	if (Audio) Audio->Play(TEXT("portal_travel"), Player->Pos, 0.25f, 1.f);
	if (To == EMCDimension::End) TickDragonFight();
	SaveLevelData();
	SavePlayer();
	UE_LOG(LogOpus55, Log, TEXT("Changed dimension %d -> %d at %s"), (int32)From, (int32)To, *Target.ToString());
}

void AMCGame::OnEndPortalEntered(AMCEntity* E)
{
	if (!E || E != Player) return;
	if (ActiveDim == EMCDimension::End)
	{
		// the exit portal: show the credits the first time
		if (!Player->bSeenCredits) { bShowCredits = true; CreditsTime = FPlatformTime::Seconds(); Player->bSeenCredits = true; }
		ChangeDimension(E, EMCDimension::Overworld, nullptr);
		return;
	}
	ChangeDimension(E, EMCDimension::End, nullptr);
}

// ---------------------------------------------------------------------------------------------------------------------
// Dragon fight

int32 AMCGame::CountEndCrystals() const
{
	const FMCWorld* W = GetMCWorld(EMCDimension::End);
	if (!W) return 0;
	int32 N = 0;
	for (const AMCEntity* E : W->Entities) if (E && !E->bRemoved && E->IsA<AMCEndCrystal>() && Cast<AMCEndCrystal>(E)->bSpike) ++N;
	return N;
}

void AMCGame::TickDragonFight()
{
	FMCWorld* W = GetMCWorld(EMCDimension::End);
	if (!W || ActiveDim != EMCDimension::End || !Player) return;
	DragonFight.CrystalsAlive = CountEndCrystals();
	const bool bDragonAlive = DragonFight.Dragon.IsValid() && !DragonFight.Dragon->bRemoved;
	if (!DragonFight.bDragonKilled && !bDragonAlive && FVector::Dist2D(Player->Pos, FVector::ZeroVector) < 300.0)
	{
		// first visit (or reload): the dragon circles the main island
		if (AMCEnderDragon* D = Cast<AMCEnderDragon>(SpawnMob(W, TEXT("ender_dragon"), FVector(0, 0, 128), false)))
		{
			D->bPersistent = true;
			DragonFight.Dragon = D;
			DragonFight.bDragonSpawned = true;
			SpawnExitPortal(false);
		}
	}
	// respawning the dragon: 4 end crystals on the exit portal
	if (DragonFight.bDragonKilled && !bDragonAlive && GameTime % 20 == 0)
	{
		const FMCEndGen* G = static_cast<const FMCEndGen*>(W->Generator.Get());
		const int32 PZ = G ? G->ExitPortalZ() : 64;
		int32 Placed = 0;
		for (const AMCEntity* E : W->Entities)
		{
			const AMCEndCrystal* C = Cast<AMCEndCrystal>(E);
			if (!C || C->bRemoved || C->bSpike) continue;
			const FVector P = C->Pos;
			if (FMath::Abs(P.Z - (PZ + 1)) < 1.5 && FVector::Dist2D(P, FVector(0.5, 0.5, 0)) < 4.5 && FVector::Dist2D(P, FVector(0.5, 0.5, 0)) > 2.0) ++Placed;
		}
		if (Placed >= 4)
		{
			DragonFight.bDragonKilled = false;
			DragonFight.bPreviouslyKilled = true;
			for (AMCEntity* E : TArray<AMCEntity*>(W->Entities)) if (AMCEndCrystal* C = Cast<AMCEndCrystal>(E)) if (!C->bSpike) C->Hurt(FMCDamage::Of(TEXT("generic")), 1.f);
			SpawnExitPortal(false);
			AddChat(TEXT("The Ender Dragon has been summoned again!"));
		}
	}
}

void AMCGame::OnDragonKilled()
{
	DragonFight.bDragonKilled = true;
	DragonFight.Dragon.Reset();
	SpawnExitPortal(true);
	FMCWorld* W = GetMCWorld(EMCDimension::End);
	if (W && !DragonFight.bEggPlaced)
	{
		const FMCEndGen* G = static_cast<const FMCEndGen*>(W->Generator.Get());
		const int32 PZ = G ? G->ExitPortalZ() : 64;
		W->SetState(FMCBlockPos(0, 0, PZ + 4), FMCBlocks::FindState(TEXT("dragon_egg")), MCSet_Default);
		DragonFight.bEggPlaced = true;
	}
	SpawnNextGateway();
	DragonFight.bPreviouslyKilled = true;
	ShowTitle(TEXT("Free the End"), TEXT("The Ender Dragon has been slain"), 5.0);
	AddChat(TEXT("[Advancement] Free the End"));
	SaveLevelData();
}

void AMCGame::SpawnExitPortal(bool bActive)
{
	FMCWorld* W = GetMCWorld(EMCDimension::End);
	if (!W) return;
	const FMCEndGen* G = static_cast<const FMCEndGen*>(W->Generator.Get());
	const int32 Z = G ? G->ExitPortalZ() : 64;
	W->ForceLoadArea(FMCChunkPos(0, 0), 1, 10.0);
	const FMCState Bedrock = FMCBlocks::C.Bedrock;
	const FMCState EndPortal = FMCBlocks::FindState(TEXT("end_portal"));
	for (int32 x = -4; x <= 4; ++x)
		for (int32 y = -4; y <= 4; ++y)
		{
			const float D2 = (float)(x * x + y * y);
			if (D2 > 12.5f) continue;
			W->SetState(FMCBlockPos(x, y, Z - 1), Bedrock, MCSet_Default);
			const bool bRim = D2 > 6.25f;
			W->SetState(FMCBlockPos(x, y, Z), bRim ? Bedrock : (bActive ? EndPortal : 0), MCSet_Default);
			for (int32 h = 1; h <= 3; ++h) if (!(x == 0 && y == 0)) W->SetState(FMCBlockPos(x, y, Z + h), 0, MCSet_Default);
		}
	for (int32 h = 0; h <= 3; ++h) W->SetState(FMCBlockPos(0, 0, Z + h), Bedrock, MCSet_Default);
	// torches on the central pillar
	static const EMCFace Sides[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
	for (int32 i = 0; i < 4; ++i)
	{
		const FMCBlockPos P = FMCBlockPos(0, 0, Z + 2).Offset(Sides[i]);
		if (const FMCBlock* T = FMCBlocks::Find(TEXT("torch"))) W->SetState(P, T->State((uint8)(1 + i)), MCSet_Default);
	}
}

void AMCGame::SpawnNextGateway()
{
	FMCWorld* W = GetMCWorld(EMCDimension::End);
	if (!W || DragonFight.GatewaysSpawned >= 20) return;
	const FMCEndGen* G = static_cast<const FMCEndGen*>(W->Generator.Get());
	if (!G) return;
	const TArray<FMCBlockPos> Gates = G->GatewayPositions();
	if (!Gates.IsValidIndex(DragonFight.GatewaysSpawned)) return;
	const FMCBlockPos P = Gates[DragonFight.GatewaysSpawned++];
	W->ForceLoadArea(FMCChunkPos::FromBlock(P), 1, 10.0);
	const FMCState Bedrock = FMCBlocks::C.Bedrock;
	const FMCState Gate = FMCBlocks::FindState(TEXT("end_gateway"));
	for (int32 dz = -2; dz <= 2; ++dz)
		for (int32 dx = -1; dx <= 1; ++dx)
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				const FMCBlockPos B(P.X + dx, P.Y + dy, P.Z + dz);
				const bool bCore = dx == 0 && dy == 0 && dz == 0;
				const bool bCap = dx == 0 && dy == 0 && FMath::Abs(dz) == 1;
				const bool bFrame = (dx == 0 && dy == 0 && FMath::Abs(dz) == 2) || (FMath::Abs(dx) + FMath::Abs(dy) == 1 && dz == 0);
				W->SetState(B, bCore ? Gate : (bCap || bFrame ? Bedrock : 0), MCSet_Default);
			}
	// link the gateway to the outer islands (~1000 blocks out along its direction)
	if (TSharedPtr<FMCBlockEntity> BE = W->GetBlockEntityShared(P))
	{
		if (BE->Type == EMCBlockEntityType::EndGateway)
		{
			FMCEndGatewayEntity* EG = static_cast<FMCEndGatewayEntity*>(BE.Get());
			const FVector Dir = FVector(P.X, P.Y, 0).GetSafeNormal();
			const FVector Far = Dir * 1024.0;
			EG->Exit = FMCBlockPos(MC::FloorToInt(Far.X), MC::FloorToInt(Far.Y), 75);
			EG->bHasExit = true;
		}
	}
	W->PlaySound(TEXT("end_gateway_spawn"), FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5), 5.f, 1.f);
}
