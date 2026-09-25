// AMCGame core: registries, world lifecycle (title panorama, create / load / save), the fixed 20 Hz loop,
// entity management, messages and effect routing.
#include "Game/MCGame.h"
#include "Game/MCGameMode.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"
#include "World/MCChunk.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCGenCommon.h"
#include "Gen/MCBiomes.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "Items/MCItems.h"
#include "Crafting/MCRecipes.h"
#include "Render/MCRig.h"
#include "Render/MCVoxelRenderer.h"
#include "Render/MCParticles.h"
#include "Audio/MCAudio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Blocks/MCBlockBehavior.h"
#include "Game/MCValidationTour.h"

namespace
{
	TWeakObjectPtr<AMCGame> GGame;
	constexpr int32 LevelVersion = 2;
}

AMCGame::AMCGame()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AMCGame* AMCGame::Get(const UObject* WorldContext)
{
	if (GGame.IsValid()) return GGame.Get();
	if (!WorldContext) return nullptr;
	UWorld* W = WorldContext->GetWorld();
	if (!W) return nullptr;
	for (TActorIterator<AMCGame> It(W); It; ++It) { GGame = *It; return *It; }
	return nullptr;
}

UMCGameInstance* AMCGame::GetMCInstance() const
{
	return Cast<UMCGameInstance>(UGameplayStatics::GetGameInstance(this));
}

void AMCGame::BeginPlay()
{
	Super::BeginPlay();
	GGame = this;
	const double T0 = FPlatformTime::Seconds();
	FMCTextures::Init();
	FMCBlocks::Init();
	FMCItems::Init();
	FMCGenBlocks::Get();
	FMCRecipes::Init();
	MCMobs::Init();
	MCRigs::Init();
	UE_LOG(LogOpus55, Log, TEXT("Registries ready in %.1f ms"), (FPlatformTime::Seconds() - T0) * 1000.0);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Renderer = GetWorld()->SpawnActor<AMCVoxelRenderer>(AMCVoxelRenderer::StaticClass(), FTransform::Identity, SP);
	Audio = NewObject<UMCAudio>(this);
	Audio->Init(this);
	Particles = NewObject<UMCParticles>(this, TEXT("Particles"));
	Particles->SetupAttachment(RootComponent);
	Particles->RegisterComponent();
	SetupEnvironment();
	ApplyOptions();

	// command line / validation runs start straight into a world
	FString WorldArg;
	UMCGameInstance* GI = GetMCInstance();
	const bool bCmdWorld = FParse::Value(FCommandLine::Get(), TEXT("Opus55World="), WorldArg);
	if (GI && !GI->PendingWorld.IsEmpty())
	{
		StartWorld(GI->PendingWorld, GI->PendingSeed, GI->bPendingLoad, (EMCGameMode)GI->PendingMode);
		GI->PendingWorld.Empty();
	}
	else if (bCmdWorld || (GI && GI->bAutoStart))
	{
		uint64 S = 0;
		FString SeedStr;
		if (FParse::Value(FCommandLine::Get(), TEXT("Opus55Seed="), SeedStr)) S = FCString::Strtoui64(*SeedStr, nullptr, 10);
		if (WorldArg.IsEmpty()) WorldArg = TEXT("Opus 5.5 Test World");
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Worlds"), WorldArg, TEXT("level.dat"));
		StartWorld(WorldArg, S, IFileManager::Get().FileExists(*Dir), EMCGameMode::Creative);
	}
	else
	{
		StartTitleScreen();
	}
	// scripted screenshot tour for validation runs
	if (AMCValidationTour::IsRequested()) GetWorld()->SpawnActor<AMCValidationTour>(AMCValidationTour::StaticClass(), FTransform::Identity, SP);
}

void AMCGame::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bWorldReady && !bTitleScreen) SaveWorld(true);
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d)
	{
		if (Worlds[d]) { Worlds[d]->UnloadAll(false); }
	}
	if (GGame.Get() == this) GGame.Reset();
	Super::EndPlay(Reason);
}

void AMCGame::ApplyOptions()
{
	if (const UMCGameInstance* GI = GetMCInstance())
	{
		RenderDistance = FMath::Clamp(GI->Options.RenderDistance, 2, 32);
		SimulationDistance = FMath::Clamp(GI->Options.SimulationDistance, 4, 12);
		if (Audio) { Audio->MasterVolume = GI->Options.MasterVolume; Audio->MusicVolume = GI->Options.MusicVolume; }
		if (Player) Player->FOVOverride = GI->Options.FOV;
	}
	if (Renderer) Renderer->RenderDistance = RenderDistance;
}

// ---------------------------------------------------------------------------------------------------------------------
// Worlds

FMCWorld* AMCGame::EnsureWorld(EMCDimension D)
{
	const int32 I = (int32)D;
	if (!Worlds[I])
	{
		Worlds[I] = MakeUnique<FMCWorld>(D, Seed, this);
		Worlds[I]->SaveDir = SaveRoot;
		Worlds[I]->RandomTickSpeed = Rules.RandomTickSpeed;
		if (!SaveRoot.IsEmpty()) IFileManager::Get().MakeDirectory(*FPaths::Combine(SaveRoot, FString::Printf(TEXT("DIM%d"), I)), true);
	}
	return Worlds[I].Get();
}

void AMCGame::StartTitleScreen()
{
	// never leave a player or entities pointing into worlds that are about to be freed
	if (Player) { Player->Destroy(); Player = nullptr; }
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d)
	{
		if (!Worlds[d]) continue;
		for (AMCEntity* E : TArray<AMCEntity*>(Worlds[d]->Entities)) if (E) E->Destroy();
		Worlds[d]->Entities.Reset();
	}
	bTitleScreen = true;
	bWorldReady = false;
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d) Worlds[d].Reset();
	// a live panorama of a generated world behind the menus
	Seed = 0x5EEDA57Aull;
	SaveRoot.Empty();
	ActiveDim = EMCDimension::Overworld;
	FMCWorld* W = EnsureWorld(EMCDimension::Overworld);
	W->bActive = true;
	if (Renderer) Renderer->SetWorld(W);
	if (Particles) Particles->SetWorld(W);
	WorldSpawn = W->Generator->FindSpawn();
	DayTime = 3000;
	if (!TitleCamera)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TitleCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP);
		TitleCamera->GetCameraComponent()->SetFieldOfView(80.f);
		TitleCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	TitleCamera->SetActorLocation(FVector(WorldSpawn.X + 0.5, WorldSpawn.Y + 0.5, WorldSpawn.Z + 18) * MC::BlockSize);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTarget(TitleCamera);
}

bool AMCGame::StartWorld(const FString& Name, uint64 InSeed, bool bLoad, EMCGameMode Mode)
{
	// tear down whatever is running (title panorama or another world)
	if (bWorldReady && !bTitleScreen) SaveWorld(true);
	if (Player) { Player->Destroy(); Player = nullptr; }
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d)
	{
		if (Worlds[d])
		{
			for (AMCEntity* E : TArray<AMCEntity*>(Worlds[d]->Entities)) if (E) E->Destroy();
			Worlds[d]->Entities.Reset();
			Worlds[d]->UnloadAll(false);
			Worlds[d].Reset();
		}
	}
	bTitleScreen = false;
	bWorldReady = false;
	bLoading = true;
	LoadingText = TEXT("Generating world");
	WorldName = Name.IsEmpty() ? FString(TEXT("New World")) : Name;
	SaveRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Worlds"), WorldName);
	IFileManager::Get().MakeDirectory(*SaveRoot, true);
	Seed = InSeed != 0 ? InSeed : (uint64)FPlatformTime::Cycles64() ^ 0x9E3779B97F4A7C15ull;
	DefaultGameMode = Mode;
	DayTime = 1000; GameTime = 0;
	bRaining = bThundering = false;
	RainTime = 12000 + Rand.NextInt(168000); ThunderTime = 12000 + Rand.NextInt(168000);
	DragonFight = FMCDragonFight();
	ActiveDim = EMCDimension::Overworld;
	bool bHaveLevel = false;
	if (bLoad)
	{
		const FString LevelPath = FPaths::Combine(SaveRoot, TEXT("level.dat"));
		if (IFileManager::Get().FileExists(*LevelPath)) { LoadLevelData(); bHaveLevel = true; }
	}
	Rand.SetSeed(Seed ^ 0xA57A);
	FMCWorld* W = EnsureWorld(ActiveDim);
	W->bActive = true;
	if (Renderer) Renderer->SetWorld(W);
	if (Particles) Particles->SetWorld(W);
	if (!bHaveLevel)
	{
		FMCWorld* OW = EnsureWorld(EMCDimension::Overworld);
		WorldSpawn = OW->Generator->FindSpawn();
	}

	// the player
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Player = GetWorld()->SpawnActor<AMCPlayer>(AMCPlayer::StaticClass(), FTransform::Identity, SP);
	Player->Game = this;
	Player->EntityId = NextEntityId++;
	Player->GameMode = DefaultGameMode;
	const bool bHavePlayer = bHaveLevel && LoadPlayer();
	if (bHavePlayer && Player->World != nullptr) {}
	FMCWorld* PW = EnsureWorld(ActiveDim);
	PW->RegisterEntity(Player);
	Player->InitEntity();
	if (!bHavePlayer) SetupNewPlayer();
	if (UMCGameInstance* GI = GetMCInstance()) Player->FOVOverride = GI->Options.FOV;

	// generate the area around the player before handing over control
	LoadingText = TEXT("Loading terrain");
	const FVector Center = bHavePlayer ? Player->Pos : FVector(WorldSpawn.X + 0.5, WorldSpawn.Y + 0.5, 100);
	PW->ForceLoadArea(FMCChunkPos::FromBlock(MC::FloorToInt(Center.X), MC::FloorToInt(Center.Y)), 3, 20.0);
	if (!bHavePlayer) PlacePlayerAtSpawn();
	else Player->SetPosition(Player->Pos, true);
	bLoadingTerrain = false;
	bLoading = false;
	bWorldReady = true;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTarget(Player);
	if (ActiveDim == EMCDimension::End) DragonFight.bDragonSpawned = DragonFight.bDragonSpawned; // dragon handled by TickDragonFight
	AddChat(FString::Printf(TEXT("Loaded world \"%s\" (seed %llu)"), *WorldName, Seed));
	SaveLevelData();
	UE_LOG(LogOpus55, Log, TEXT("World '%s' started, seed %llu, mode %d"), *WorldName, Seed, (int32)DefaultGameMode);
	return true;
}

void AMCGame::SetupNewPlayer()
{
	Player->SetGameMode(DefaultGameMode);
	Player->Health = Player->MaxHealth;
	Player->FoodLevel = 20;
	Player->Saturation = 5.f;
	Player->Selected = 0;
	if (DefaultGameMode == EMCGameMode::Creative)
	{
		// a useful starter hotbar for the creative default
		const TCHAR* Kit[9] = { TEXT("grass_block"), TEXT("oak_planks"), TEXT("stone_bricks"), TEXT("glass"), TEXT("torch"), TEXT("oak_log"), TEXT("diamond_pickaxe"), TEXT("water_bucket"), TEXT("pig_spawn_egg") };
		for (int32 i = 0; i < 9; ++i) Player->Inventory[i] = FMCItemStack::Of(Kit[i], FMCItemStack::Of(Kit[i], 1).MaxStack());
	}
}

void AMCGame::PlacePlayerAtSpawn()
{
	FMCWorld* W = EnsureWorld(EMCDimension::Overworld);
	const FMCBlockPos Safe = W->FindSafeSpawn(WorldSpawn.X, WorldSpawn.Y);
	WorldSpawn = Safe;
	Player->SetPosition(FVector(Safe.X + 0.5, Safe.Y + 0.5, Safe.Z), true);
	Player->ViewYaw = Player->Yaw = 0.f;
	Player->ViewPitch = Player->Pitch = 0.f;
}

void AMCGame::QuitToTitle()
{
	if (bWorldReady && !bTitleScreen) SaveWorld(true);
	if (Player) { Player->Destroy(); Player = nullptr; }
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d)
	{
		if (!Worlds[d]) continue;
		for (AMCEntity* E : TArray<AMCEntity*>(Worlds[d]->Entities)) if (E) E->Destroy();
		Worlds[d]->Entities.Reset();
		Worlds[d]->UnloadAll(false);
		Worlds[d].Reset();
	}
	if (Audio) Audio->StopAll();
	ChatLog.Reset(); ChatTimes.Reset();
	StartTitleScreen();
}

// ---------------------------------------------------------------------------------------------------------------------
// Persistence

void AMCGame::SaveWorld(bool bAllChunks)
{
	if (SaveRoot.IsEmpty() || bTitleScreen) return;
	const double T0 = FPlatformTime::Seconds();
	SaveLevelData();
	SavePlayer();
	for (int32 d = 0; d < (int32)EMCDimension::Count; ++d) if (Worlds[d]) Worlds[d]->SaveAllChunks();
	UE_LOG(LogOpus55, Log, TEXT("Saved world '%s' in %.1f ms"), *WorldName, (FPlatformTime::Seconds() - T0) * 1000.0);
}

void AMCGame::SaveLevelData()
{
	if (SaveRoot.IsEmpty()) return;
	FBufferArchive Ar;
	int32 Magic = 0x41535441, Version = LevelVersion;
	Ar << Magic << Version;
	Ar << WorldName << Seed << DayTime << GameTime << bRaining << bThundering << RainTime << ThunderTime << ClearWeatherTime;
	uint8 Diff = (uint8)Difficulty, Mode = (uint8)DefaultGameMode, Dim = (uint8)ActiveDim;
	Ar << Diff << Mode << Dim << bHardcore << bAllowCheats << WorldSpawn;
	Ar << Rules.bDoDaylightCycle << Rules.bDoWeatherCycle << Rules.bDoMobSpawning << Rules.bKeepInventory << Rules.bMobGriefing << Rules.bDoFireTick
		<< Rules.bNaturalRegeneration << Rules.bShowCoordinates << Rules.bDoImmediateRespawn << Rules.bFallDamage << Rules.bDrowningDamage
		<< Rules.bFireDamage << Rules.bDoTileDrops << Rules.bDoMobLoot << Rules.RandomTickSpeed << Rules.SpawnRadius;
	Ar << DragonFight.bDragonKilled << DragonFight.bPreviouslyKilled << DragonFight.bDragonSpawned << DragonFight.bEggPlaced << DragonFight.GatewaysSpawned;
	FFileHelper::SaveArrayToFile(Ar, *FPaths::Combine(SaveRoot, TEXT("level.dat")));
}

void AMCGame::LoadLevelData()
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *FPaths::Combine(SaveRoot, TEXT("level.dat")))) return;
	FMemoryReader Ar(Data);
	int32 Magic = 0, Version = 0;
	Ar << Magic << Version;
	if (Magic != 0x41535441) return;
	FString Name;
	Ar << Name << Seed << DayTime << GameTime << bRaining << bThundering << RainTime << ThunderTime << ClearWeatherTime;
	uint8 Diff = 2, Mode = 1, Dim = 0;
	Ar << Diff << Mode << Dim << bHardcore << bAllowCheats << WorldSpawn;
	Difficulty = (EMCDifficulty)Diff; DefaultGameMode = (EMCGameMode)Mode; ActiveDim = (EMCDimension)FMath::Min<uint8>(Dim, 2);
	Ar << Rules.bDoDaylightCycle << Rules.bDoWeatherCycle << Rules.bDoMobSpawning << Rules.bKeepInventory << Rules.bMobGriefing << Rules.bDoFireTick
		<< Rules.bNaturalRegeneration << Rules.bShowCoordinates << Rules.bDoImmediateRespawn << Rules.bFallDamage << Rules.bDrowningDamage
		<< Rules.bFireDamage << Rules.bDoTileDrops << Rules.bDoMobLoot << Rules.RandomTickSpeed << Rules.SpawnRadius;
	if (Version >= 2) Ar << DragonFight.bDragonKilled << DragonFight.bPreviouslyKilled << DragonFight.bDragonSpawned << DragonFight.bEggPlaced << DragonFight.GatewaysSpawned;
	DragonFight.bDragonSpawned = false; // the dragon entity itself is respawned by the fight manager when needed
	RainLevel = bRaining ? 1.f : 0.f;
	ThunderLevel = bThundering ? 1.f : 0.f;
}

void AMCGame::SavePlayer()
{
	if (!Player || SaveRoot.IsEmpty()) return;
	FBufferArchive Ar;
	int32 Version = 1;
	uint8 Dim = (uint8)ActiveDim;
	Ar << Version << Dim;
	Player->Serialize(Ar);
	FFileHelper::SaveArrayToFile(Ar, *FPaths::Combine(SaveRoot, TEXT("player.dat")));
}

bool AMCGame::LoadPlayer()
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *FPaths::Combine(SaveRoot, TEXT("player.dat")))) return false;
	FMemoryReader Ar(Data);
	int32 Version = 0;
	uint8 Dim = 0;
	Ar << Version << Dim;
	ActiveDim = (EMCDimension)FMath::Min<uint8>(Dim, 2);
	Player->Serialize(Ar);
	return !Ar.IsError();
}

// ---------------------------------------------------------------------------------------------------------------------
// Entities

AMCEntity* AMCGame::SpawnEntityOfClass(UClass* Cls, FMCWorld* W, const FVector& PosBlocks)
{
	if (!Cls || !W || !GetWorld()) return nullptr;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.bDeferConstruction = false;
	AMCEntity* E = GetWorld()->SpawnActor<AMCEntity>(Cls, FTransform(PosBlocks * MC::BlockSize), SP);
	if (!E) return nullptr;
	E->Game = this;
	E->World = W;
	E->EntityId = NextEntityId++;
	E->SetPosition(PosBlocks, true);
	W->RegisterEntity(E);
	// inactive dimensions do not render
	if (W != ActiveWorld()) E->SetActorHiddenInGame(true);
	return E;
}

AMCEntity* AMCGame::SpawnMob(FMCWorld* W, FName MobId, const FVector& PosBlocks, bool bNatural, int32 Variant)
{
	const FMCMobDef* Def = MCMobs::Find(MobId);
	if (!Def || !W) return nullptr;
	UClass* Cls = AMCMob::StaticClass();
	if (Def->AI == EMCMobAI::Dragon) Cls = AMCEnderDragon::StaticClass();
	else if (Def->AI == EMCMobAI::Wither) Cls = AMCWither::StaticClass();
	AMCMob* M = Cast<AMCMob>(SpawnEntityOfClass(Cls, W, PosBlocks));
	if (!M) return nullptr;
	M->SetDefinition(Def, Variant);
	M->bNaturalSpawn = bNatural;
	M->bPersistent = !bNatural || Def->Category == EMCMobCategory::Creature;
	M->Yaw = M->BodyYaw = W->Rand.NextFloat() * 360.f;
	M->InitEntity();
	if (Def->AI == EMCMobAI::Dragon) DragonFight.Dragon = Cast<AMCEnderDragon>(M);
	return M;
}

void AMCGame::DestroyEntity(AMCEntity* E)
{
	if (!E) return;
	if (E->World) E->World->UnregisterEntity(E);
	E->bRemoved = true;
	E->Destroy();
}

void AMCGame::TickEntities(FMCWorld& W)
{
	TArray<AMCEntity*> List = W.Entities;
	for (AMCEntity* E : List)
	{
		if (!E || !IsValid(E) || E->bRemoved) continue;
		// entities in chunks that are not ready are frozen
		if (E != Player && !W.IsReadyAt(E->BlockPos())) continue;
		if (E->Vehicle.IsValid() && E->Vehicle->World == &W) {} // passengers tick after their vehicle moved (vehicle carries them)
		E->TickEntity();
	}
	for (int32 i = W.Entities.Num() - 1; i >= 0; --i)
	{
		AMCEntity* E = W.Entities[i];
		if (!E || !IsValid(E)) { W.Entities.RemoveAtSwap(i, EAllowShrinking::No); continue; }
		if (E->bRemoved && E != Player)
		{
			W.Entities.RemoveAtSwap(i, EAllowShrinking::No);
			E->Destroy();
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Main loop

void AMCGame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.25f);
	// FPS counter
	++FramesThisSecond;
	FPSTimer += DeltaSeconds;
	if (FPSTimer >= 1.0) { FPS = FramesThisSecond; FramesThisSecond = 0; FPSTimer = 0.0; if (Audio) Audio->SoundsThisSecond = 0; }

	FMCWorld* W = ActiveWorld();
	if (!W) return;

	if (bTitleScreen)
	{
		// slowly orbit the panorama
		TitleYaw += DeltaSeconds * 3.f;
		DayTime = 3000;
		W->UpdateStreaming(FVector(WorldSpawn.X, WorldSpawn.Y, 80), FMath::Min(RenderDistance, 8), 6.0);
		if (TitleCamera)
		{
			const FVector C(WorldSpawn.X + 0.5, WorldSpawn.Y + 0.5, WorldSpawn.Z + 22);
			TitleCamera->SetActorLocation(C * MC::BlockSize);
			TitleCamera->SetActorRotation(FRotator(-12.f, TitleYaw, 0.f));
			if (Renderer) Renderer->UpdateRenderer(DeltaSeconds, C);
			if (Particles) Particles->UpdateParticles(DeltaSeconds, C * MC::BlockSize, TitleCamera->GetActorRotation());
		}
		UpdateEnvironment(DeltaSeconds);
		if (Audio) Audio->Tick(DeltaSeconds);
		return;
	}
	if (!bWorldReady || !Player) return;

	// fixed 20 Hz simulation
	if (!bPaused)
	{
		TickAccumulator += DeltaSeconds;
		int32 Steps = 0;
		while (TickAccumulator >= MC::TickTime && Steps < 5)
		{
			const double T0 = FPlatformTime::Seconds();
			FixedTick();
			const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
			LastTickMs = Ms;
			AvgTickMs = AvgTickMs * 0.95 + Ms * 0.05;
			TickHistory.Add(Ms);
			if (TickHistory.Num() > 120) TickHistory.RemoveAt(0, TickHistory.Num() - 120, EAllowShrinking::No);
			TickAccumulator -= MC::TickTime;
			++Steps;
			W = ActiveWorld(); // may change during a dimension switch
			if (!W || !Player) return;
		}
		if (Steps == 5) TickAccumulator = 0.0; // can't keep up: drop time instead of spiralling
	}
	PartialTick = bPaused ? 1.f : (float)(TickAccumulator / MC::TickTime);

	// streaming and rendering
	StreamTimer += DeltaSeconds;
	W->UpdateStreaming(Player->Pos, RenderDistance, 4.0);
	for (AMCEntity* E : W->Entities)
	{
		if (E && !E->bRemoved && IsValid(E)) E->UpdateVisual(PartialTick, DeltaSeconds);
	}
	const FVector CamBlocks = Player->Camera ? Player->Camera->GetComponentLocation() * MC::InvBlockSize : Player->GetEyePos();
	if (Renderer)
	{
		Renderer->RenderDistance = RenderDistance;
		Renderer->UpdateRenderer(DeltaSeconds, CamBlocks);
		// selection outline + crack stage
		FMCRayHit Hit;
		AMCEntity* Target = nullptr;
		const bool bAny = !Player->IsMenuOpen() && Player->IsAlive() && !Player->IsSpectator() && Player->GetTarget(Hit, Target);
		Renderer->SetSelection(bAny && Hit.bHit && !Target, Hit.Pos, Hit.State, Player->bMining && Player->MiningPos == Hit.Pos ? Player->MiningProgress : 0.f);
	}
	if (Particles && Player->Camera) Particles->UpdateParticles(DeltaSeconds, Player->Camera->GetComponentLocation(), Player->Camera->GetComponentRotation());
	UpdateEnvironment(DeltaSeconds);
	if (Audio) Audio->Tick(DeltaSeconds);
	if (Shake > 0.f) Shake = FMath::Max(0.f, Shake - DeltaSeconds * 1.5f);
	LightningFlash = FMath::Max(0.f, LightningFlash - DeltaSeconds * 3.f);
	// expire UI messages
	const double Now = FPlatformTime::Seconds();
	(void)Now;
}

void AMCGame::FixedTick()
{
	FMCWorld* W = ActiveWorld();
	if (!W || !Player) return;
	++GameTime;
	TickTimeAndWeather();
	W->RandomTickSpeed = Rules.RandomTickSpeed;
	W->Tick(Player->Pos);
	TickEntities(*W);
	if (bPendingDimChange) { bPendingDimChange = false; return; }
	if (Rules.bDoMobSpawning) TickSpawning(*W);
	if (ActiveDim == EMCDimension::End) TickDragonFight();

	// random display ticks (particles near the player: torches, portals, drips...)
	const FMCBlockPos PP = Player->BlockPos();
	for (int32 i = 0; i < 667; ++i)
	{
		const int32 R = i < 333 ? 16 : 32;
		const FMCBlockPos P(PP.X + Rand.Range(-R, R), PP.Y + Rand.Range(-R, R), PP.Z + Rand.Range(-R, R));
		const FMCState S = W->GetState(P);
		if (S == 0) continue;
		FMCBlocks::GetByState(S).Behavior->OnAnimateTick(*W, P, S, Rand);
	}

	// sleeping through the night
	if (Player->bSleeping && Player->SleepCounter >= 100 && !IsDay())
	{
		DayTime = (DayTime / 24000 + 1) * 24000;
		if (bRaining || bThundering) SetWeather(0, 12000 + Rand.NextInt(168000));
		Player->WakeUp(true);
	}
	// autosave every 5 minutes
	if (++AutosaveTimer >= 6000) { AutosaveTimer = 0; SaveWorld(false); }
}

void AMCGame::TickTimeAndWeather()
{
	if (Rules.bDoDaylightCycle) ++DayTime;
	PrevRainLevel = RainLevel;
	PrevThunderLevel = ThunderLevel;
	if (Rules.bDoWeatherCycle && ActiveDim == EMCDimension::Overworld)
	{
		if (ClearWeatherTime > 0)
		{
			--ClearWeatherTime;
			ThunderTime = bThundering ? 0 : 1;
			RainTime = bRaining ? 0 : 1;
			bThundering = bRaining = false;
		}
		else
		{
			if (ThunderTime > 0) { if (--ThunderTime == 0) bThundering = !bThundering; }
			else ThunderTime = bThundering ? Rand.Range(3600, 15600) : Rand.Range(12000, 180000);
			if (RainTime > 0) { if (--RainTime == 0) bRaining = !bRaining; }
			else RainTime = bRaining ? Rand.Range(12000, 24000) : Rand.Range(12000, 180000);
		}
	}
	RainLevel = FMath::Clamp(RainLevel + (bRaining ? 0.01f : -0.01f), 0.f, 1.f);
	ThunderLevel = FMath::Clamp(ThunderLevel + (bThundering && bRaining ? 0.01f : -0.01f), 0.f, 1.f);

	FMCWorld* W = ActiveWorld();
	if (!W || W->Dim != EMCDimension::Overworld || !Player) return;
	// lightning during thunderstorms
	if (bRaining && bThundering && ThunderLevel > 0.9f && Rand.NextInt(350) == 0)
	{
		const int32 X = MC::FloorToInt(Player->Pos.X) + Rand.Range(-96, 96);
		const int32 Y = MC::FloorToInt(Player->Pos.Y) + Rand.Range(-96, 96);
		if (W->IsReadyAt(FMCBlockPos(X, Y, 64)))
		{
			const FMCBlockPos Top(X, Y, W->GetHeight(X, Y) + 1);
			if (IsRainingAt(Top)) StrikeLightning(W, FVector(X + 0.5, Y + 0.5, Top.Z), false);
		}
	}
	// snow and ice accumulate in cold biomes while it rains
	if (RainLevel > 0.2f)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 X = MC::FloorToInt(Player->Pos.X) + Rand.Range(-48, 48);
			const int32 Y = MC::FloorToInt(Player->Pos.Y) + Rand.Range(-48, 48);
			if (!W->IsReadyAt(FMCBlockPos(X, Y, 64))) continue;
			const int32 Z = W->GetHeight(X, Y);
			const FMCBlockPos Top(X, Y, Z);
			const FMCBiomeDef& B = FMCBiomes::Get(W->GetBiome(Top));
			if (!B.bSnowy && B.Temperature >= 0.15f) continue;
			const FMCState S = W->GetState(Top);
			if (S == FMCBlocks::C.Water && (FMCBlocks::MetaOf(S) & 15) == 0) W->SetState(Top, FMCBlocks::C.Ice, MCSet_Default);
			else if (W->GetState(Top.Up()) == 0 && FMCBlocks::IsOpaque(S)) W->SetState(Top.Up(), FMCBlocks::C.Snow, MCSet_Default);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Time helpers

float AMCGame::GetSunAngle() const
{
	// Minecraft celestial angle (0 = noon... smoothed)
	const double T = ((DayTime % 24000) + PartialTick) / 24000.0 - 0.25;
	double F = T - FMath::FloorToDouble(T);
	const double G = 0.5 - FMath::Cos(F * PI) / 2.0;
	return (float)((F * 2.0 + G) / 3.0);
}

int32 AMCGame::GetSkyDarken() const
{
	const float A = GetSunAngle();
	float F = 1.f - (FMath::Cos(A * 2.f * PI) * 2.f + 0.5f);
	F = FMath::Clamp(F, 0.f, 1.f);
	F = 1.f - F;
	F *= 1.f - RainLevel * 5.f / 16.f;
	F *= 1.f - ThunderLevel * 5.f / 16.f;
	F = 1.f - F;
	return (int32)(F * 11.f);
}

void AMCGame::SetWeather(int32 Kind, int32 DurationTicks)
{
	switch (Kind)
	{
	case 0: ClearWeatherTime = DurationTicks; RainTime = 0; ThunderTime = 0; bRaining = false; bThundering = false; break;
	case 1: ClearWeatherTime = 0; RainTime = DurationTicks; ThunderTime = DurationTicks; bRaining = true; bThundering = false; break;
	default: ClearWeatherTime = 0; RainTime = DurationTicks; ThunderTime = DurationTicks; bRaining = true; bThundering = true; break;
	}
}

bool AMCGame::IsRainingAt(const FMCBlockPos& P) const
{
	const FMCWorld* W = ActiveWorld();
	if (!W || W->Dim != EMCDimension::Overworld || RainLevel < 0.2f) return false;
	if (!W->CanSeeSky(P)) return false;
	const FMCBiomeDef& B = FMCBiomes::Get(W->GetBiome(P));
	return !B.bDry && !B.bSnowy && B.Temperature >= 0.15f;
}

float AMCGame::GetRainAt(const FVector& PosBlocks) const
{
	return FMath::Lerp(PrevRainLevel, RainLevel, PartialTick);
}

// ---------------------------------------------------------------------------------------------------------------------
// Messages & effects

void AMCGame::AddChat(const FString& Msg)
{
	ChatLog.Add(Msg);
	ChatTimes.Add(FPlatformTime::Seconds());
	if (ChatLog.Num() > 100) { ChatLog.RemoveAt(0); ChatTimes.RemoveAt(0); }
	UE_LOG(LogOpus55, Log, TEXT("[Chat] %s"), *Msg);
}

void AMCGame::ShowActionBar(const FString& Msg, double Seconds)
{
	ActionBarText = Msg;
	ActionBarTime = FPlatformTime::Seconds() + Seconds;
}

void AMCGame::ShowTitle(const FString& Title, const FString& Sub, double Seconds)
{
	TitleText = Title;
	SubtitleText = Sub;
	TitleTime = FPlatformTime::Seconds() + Seconds;
}

void AMCGame::PlaySound(FMCWorld* W, FName Sound, const FVector& PosBlocks, float Volume, float Pitch)
{
	if (!Audio || (W && W != ActiveWorld())) return;
	Audio->Play(Sound, PosBlocks, Volume, Pitch);
}

void AMCGame::SpawnParticles(FMCWorld* W, FName Type, const FVector& PosBlocks, int32 Count, float Spread, const FVector& Vel, FColor Color)
{
	if (!Particles || (W && W != ActiveWorld())) return;
	Particles->Spawn(Type, PosBlocks, Count, Spread, Vel, Color);
}

void AMCGame::SpawnBlockParticles(FMCWorld* W, const FMCBlockPos& P, uint16 State, bool bBreak)
{
	if (!Particles || (W && W != ActiveWorld())) return;
	if (bBreak) Particles->SpawnBlockBreak(P, State);
	else if (Player) Particles->SpawnBlockHit(P, State, EMCFace::Up);
}

void AMCGame::StrikeLightning(FMCWorld* W, const FVector& PosBlocks, bool bVisualOnly)
{
	if (!W) return;
	if (AMCLightning* L = SpawnEntity<AMCLightning>(W, PosBlocks))
	{
		L->bVisualOnly = bVisualOnly;
		L->InitEntity();
		LastLightningPos = PosBlocks;
		LightningFlash = 1.f;
	}
}

void AMCGame::GetBossBars(TArray<AMCMob*>& Out) const
{
	const FMCWorld* W = ActiveWorld();
	if (!W || !Player) return;
	for (AMCEntity* E : W->Entities)
	{
		AMCMob* M = Cast<AMCMob>(E);
		if (!M || M->bRemoved || !M->Def || !M->Def->bBoss) continue;
		if (FVector::DistSquared(M->Pos, Player->Pos) > (M->IsA<AMCEnderDragon>() ? 192.0 * 192.0 : 64.0 * 64.0)) continue;
		Out.Add(M);
	}
}
