// Scripted self-test (-Opus55Tour): stages a fixed list of scenes in the running game and screenshots each one.
#include "Game/MCValidationTour.h"
#include "Game/MCGame.h"
#include "Game/MCGameMode.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCBiomes.h"
#include "Gen/MCEndGen.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCBlockBehavior.h"
#include "Render/MCVoxelRenderer.h"
#include "Render/MCRig.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UI/MCRootWidget.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "RenderTimer.h"
#include "RHI.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCValidationTour)

namespace
{
	/** Component-level state of an actor, for diagnosing visuals that do not show up. */
	FString DescribeComponents(const AActor* A, int32 Max)
	{
		if (!A) return TEXT("(none)");
		TArray<UPrimitiveComponent*> Prims;
		A->GetComponents<UPrimitiveComponent>(Prims);
		FString S = FString::Printf(TEXT("%s at %s hidden=%d, %d primitives"), *A->GetName(), *A->GetActorLocation().ToString(), A->IsHidden() ? 1 : 0, Prims.Num());
		if (const UMCRigComponent* R = A->FindComponentByClass<UMCRigComponent>())
			S += FString::Printf(TEXT(", rig %s reg=%d vis=%d at %s scale %s parts=%d"), *R->RigId.ToString(), R->IsRegistered() ? 1 : 0, R->IsVisible() ? 1 : 0,
				*R->GetComponentLocation().ToString(), *R->GetComponentScale().ToString(), R->Meshes.Num());
		int32 N = 0;
		for (const UPrimitiveComponent* P : Prims)
		{
			if (N++ >= Max) break;
			const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(P);
			const UMaterialInterface* Mat = P->GetMaterial(0);
			S += FString::Printf(TEXT("\n      %s reg=%d vis=%d hiddenInGame=%d proxy=%d at %s scale %s mesh=%s radius=%.1f mat=%s"),
				*P->GetName(), P->IsRegistered() ? 1 : 0, P->IsVisible() ? 1 : 0, P->bHiddenInGame ? 1 : 0, P->SceneProxy ? 1 : 0,
				*P->GetComponentLocation().ToString(), *P->GetComponentScale().ToString(),
				SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : TEXT("-"), P->Bounds.SphereRadius, Mat ? *Mat->GetName() : TEXT("-"));
		}
		return S;
	}
}

AMCValidationTour::AMCValidationTour()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorHiddenInGame(true);
}

bool AMCValidationTour::IsRequested()
{
	// -Opus55TitleShot: no world, just a picture of the title screen
	return FParse::Param(FCommandLine::Get(), TEXT("Opus55Tour")) || FParse::Param(FCommandLine::Get(), TEXT("Opus55TitleShot"));
}

AMCGame* AMCValidationTour::Game() const { return AMCGame::Get(this); }
AMCPlayer* AMCValidationTour::Player() const { AMCGame* G = Game(); return G ? G->Player.Get() : nullptr; }
AMCPlayerController* AMCValidationTour::Controller() const { return Cast<AMCPlayerController>(UGameplayStatics::GetPlayerController(this, 0)); }

bool AMCValidationTour::IsStreaming() const
{
	const AMCGame* G = Game();
	if (!G || !G->Renderer) return true;
	const FMCWorld* W = G->ActiveWorld();
	if (W && W->NumPendingGeneration() > 0) return true;
	if (G->Renderer->StatPendingMeshes > 0) return true;
	return GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
}

void AMCValidationTour::Cmd(const FString& Line)
{
	if (AMCGame* G = Game()) G->ExecuteCommand(Line, Player());
}

void AMCValidationTour::Place(const FVector& PosBlocks, float Yaw, float Pitch)
{
	AMCPlayer* P = Player();
	if (!P) return;
	P->TeleportTo(PosBlocks);
	P->Vel = FVector::ZeroVector;
	P->FallDistance = 0.f;
	P->ViewYaw = P->Yaw = P->PrevYaw = Yaw;
	P->ViewPitch = P->Pitch = P->PrevPitch = Pitch;
}

void AMCValidationTour::LookAt(const FVector& TargetBlocks)
{
	AMCPlayer* P = Player();
	if (!P) return;
	const FVector D = TargetBlocks - P->GetEyePos();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, FVector2D(D.X, D.Y).Size()));
	P->ViewYaw = P->Yaw = P->PrevYaw = Yaw;
	P->ViewPitch = P->Pitch = P->PrevPitch = FMath::Clamp(Pitch, -89.f, 89.f);
}

void AMCValidationTour::BuildStage()
{
	// a flat grass stage in front of the stand point (+X), cleared up to 12 blocks high
	const int32 X = FMath::FloorToInt(Stand.X), Y = FMath::FloorToInt(Stand.Y);
	const int32 X1 = X - 3, X2 = X + 16, Y1 = Y - 12, Y2 = Y + 12;
	Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d dirt"), X1, Floor - 2, Y1, X2, Floor - 1, Y2));
	Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d grass_block"), X1, Floor, Y1, X2, Floor, Y2));
	Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d air"), X1, Floor + 1, Y1, X2, Floor + 12, Y2));
}

int32 AMCValidationTour::SpawnLineup(const TArray<FName>& Mobs, double Distance, double Spacing)
{
	AMCGame* G = Game();
	FMCWorld* W = G ? G->ActiveWorld() : nullptr;
	if (!W) return 0;
	int32 Made = 0;
	for (int32 i = 0; i < Mobs.Num(); ++i)
	{
		const double Side = (i - (Mobs.Num() - 1) * 0.5) * Spacing;
		const FVector Pos(FMath::FloorToDouble(Stand.X) + 0.5 + Distance, FMath::FloorToDouble(Stand.Y) + 0.5 + Side, Floor + 1.0);
		AMCMob* M = Cast<AMCMob>(G->SpawnMob(W, Mobs[i], Pos, false));
		if (!M) { Log(FString::Printf(TEXT("    mob %s: FAILED to spawn"), *Mobs[i].ToString())); continue; }
		M->bNoAI = true;
		M->bPersistent = true;
		M->bSilent = true;
		// undead still catch fire in the morning sun (as in Minecraft); keep the hurt flash out of the line-up shots
		FMCEffectInstance Fr;
		Fr.Effect = EMCEffect::FireResistance; Fr.Duration = 1000000; Fr.Amplifier = 0;
		M->AddEffect(Fr);
		M->Yaw = M->PrevYaw = M->BodyYaw = M->PrevBodyYaw = M->HeadYaw = 180.f;   // facing the camera (-X)
		M->Pitch = M->PrevPitch = 0.f;
		Spawned.Add(M);
		++Made;
	}
	return Made;
}

void AMCValidationTour::ClearSpawned()
{
	AMCGame* G = Game();
	FMCWorld* W = G ? G->ActiveWorld() : nullptr;
	if (W)
	{
		// everything near the stage except the player: line-up mobs, dropped items, natural spawns walking in
		for (AMCEntity* E : TArray<AMCEntity*>(W->Entities))
		{
			if (!E || E == Player() || E->bRemoved) continue;
			if (FVector::DistSquared(E->Pos, Stand) < 48.0 * 48.0) E->Discard();
		}
	}
	Spawned.Reset();
}

bool AMCValidationTour::FindCavePocket(FName BiomeName, FVector& Out) const
{
	AMCGame* G = Game();
	FMCWorld* W = G ? G->ActiveWorld() : nullptr;
	AMCPlayer* P = Player();
	if (!W || !W->Generator || !P) return false;
	const EMCBiome Biome = FMCBiomes::FromName(BiomeName.ToString());
	if (Biome == EMCBiome::Count) return false;
	FMCBlockPos Found;
	if (!W->Generator->LocateBiome((uint8)Biome, P->BlockPos(), 6400, Found)) return false;
	W->ForceLoadArea(FMCChunkPos::FromBlock(Found.X, Found.Y), 2, 25.0);
	// an air pocket at least three blocks tall with a floor, under cover, inside the biome; prefer roomy ones
	double BestScore = -1.0;
	for (int32 dx = -28; dx <= 28; dx += 2)
	{
		for (int32 dy = -28; dy <= 28; dy += 2)
		{
			const int32 X = Found.X + dx, Y = Found.Y + dy;
			const int32 Top = W->GetHeight(X, Y);
			for (int32 Z = FMath::Min(Top - 4, 60); Z > -56; --Z)
			{
				const FMCBlockPos A(X, Y, Z);
				if (!W->IsAir(A) || !W->IsAir(FMCBlockPos(X, Y, Z + 1)) || !W->IsAir(FMCBlockPos(X, Y, Z + 2))) continue;
				if (!FMCBlocks::IsSolid(W->GetState(FMCBlockPos(X, Y, Z - 1)))) continue;
				if (W->GetBiome(A) != (uint8)Biome) continue;
				int32 Room = 0;
				for (int32 h = 3; h < 12 && W->IsAir(FMCBlockPos(X, Y, Z + h)); ++h) ++Room;
				int32 Open = 0;
				for (int32 r = 2; r <= 8; r += 2)
				{
					Open += W->IsAir(FMCBlockPos(X + r, Y, Z + 1)) + W->IsAir(FMCBlockPos(X - r, Y, Z + 1))
						+ W->IsAir(FMCBlockPos(X, Y + r, Z + 1)) + W->IsAir(FMCBlockPos(X, Y - r, Z + 1));
				}
				const double Score = Room * 2.0 + Open;
				if (Score > BestScore) { BestScore = Score; Out = FVector(X + 0.5, Y + 0.5, Z); }
				break;
			}
		}
	}
	return BestScore >= 0.0;
}

void AMCValidationTour::CloseScreens()
{
	AMCPlayer* P = Player();
	AMCPlayerController* PC = Controller();
	if (P && P->IsMenuOpen()) P->CloseMenu();
	if (PC) PC->UpdateInputMode();
}

void AMCValidationTour::Log(const FString& Line)
{
	UE_LOG(LogOpus55, Display, TEXT("OPUS55_TOUR %s"), *Line);
	Report.Add(Line);
}

FString AMCValidationTour::Stats() const
{
	const AMCGame* G = Game();
	const AMCPlayer* P = Player();
	if (!G || !G->Renderer) return TEXT("(no game)");
	const FMCWorld* W = G->ActiveWorld();
	// engine frame split (game thread, render thread, GPU) to tell what bounds the frame rate
	const double GT = FPlatformTime::ToMilliseconds(GGameThreadTime), RT = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const double GPU = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	// chunk sections actually drawn during the last 0.2 s (tells culling apart from shading problems)
	const int32 Drawn = G->Renderer->CountDrawnSections(0.2f);
	return FString::Printf(TEXT("fps=%d gt=%.1f rt=%.1f gpu=%.1f tick=%.2fms chunks=%d meshes=%d drawn=%d tris=%d pending=%d entities=%d lights=%d dim=%d pos=(%.1f, %.1f, %.1f)"),
		G->FPS, GT, RT, GPU, G->AvgTickMs, W ? W->Chunks.Num() : 0, G->Renderer->StatComponents, Drawn, G->Renderer->StatTriangles, G->Renderer->StatPendingMeshes,
		W ? W->Entities.Num() : 0, G->Renderer->StatLights, (int32)G->ActiveDim, P ? P->Pos.X : 0.0, P ? P->Pos.Y : 0.0, P ? P->Pos.Z : 0.0);
}

int32 AMCValidationTour::CountBlocks(const FIntVector& Min, const FIntVector& Max, bool bAir, uint16 BlockId) const
{
	const AMCGame* G = Game();
	const FMCWorld* W = G ? G->ActiveWorld() : nullptr;
	if (!W) return 0;
	int32 N = 0;
	for (int32 X = Min.X; X <= Max.X; ++X)
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
			{
				const FMCState St = W->GetState(FMCBlockPos(X, Y, Z));
				if (bAir ? St == 0 : FMCBlocks::Info(St).Block == BlockId) ++N;
			}
	return N;
}

void AMCValidationTour::BuildSteps()
{
	bLoadCheck = FParse::Param(FCommandLine::Get(), TEXT("Opus55LoadCheck"));
	auto Add = [this](const TCHAR* Name, float Settle, bool bStream, TFunction<bool(AMCValidationTour&)> Enter, TFunction<void(AMCValidationTour&)> Leave = nullptr)
	{
		FStep S;
		S.Name = Name;
		S.Settle = Settle;
		S.bWaitStreaming = bStream;
		S.MaxWait = bStream ? 45.f : 5.f;
		S.Enter = MoveTemp(Enter);
		S.Leave = MoveTemp(Leave);
		Steps.Add(MoveTemp(S));
	};
	auto Fly = [](AMCValidationTour& T, bool bOn) { if (AMCPlayer* P = T.Player()) { P->bFlying = bOn && P->bMayFly; } };

	if (bLoadCheck)
	{
		// second launch on the saved tour world: where did the player come back, with what
		Add(TEXT("L1_reloaded_world"), 2.f, true, [](AMCValidationTour& T)
		{
			const AMCGame* G = T.Game();
			const AMCPlayer* P = T.Player();
			if (!G || !P) return false;
			const FMCItemStack& H = P->HeldConst();
			T.Log(FString::Printf(TEXT("    reloaded: dim=%d pos=(%.1f, %.1f, %.1f) mode=%d held=%s x%d"), (int32)G->ActiveDim, P->Pos.X, P->Pos.Y, P->Pos.Z,
				(int32)P->GameMode, H.IsEmpty() ? TEXT("-") : *H.Item().Name.ToString(), H.IsEmpty() ? 0 : H.Count));
			return true;
		});
		return;
	}

	// ---- overworld terrain
	Add(TEXT("01_spawn_view"), 2.f, true, [Fly](AMCValidationTour& T)
	{
		Fly(T, true);
		T.Place(T.Stand, 30.f, -6.f);
		return true;
	});
	Add(TEXT("02_spawn_reverse"), 2.f, true, [](AMCValidationTour& T) { T.Place(T.Stand, 210.f, -4.f); return true; });
	Add(TEXT("03_aerial"), 3.f, true, [](AMCValidationTour& T) { T.Place(T.Stand + FVector(-6, -6, 34), 45.f, -32.f); return true; });
	Add(TEXT("04_debug_overlay"), 1.5f, false, [](AMCValidationTour& T)
	{
		T.Place(T.Stand, 30.f, -6.f);
		if (AMCPlayerController* PC = T.Controller()) PC->bShowDebug = true;
		return true;
	}, [](AMCValidationTour& T) { if (AMCPlayerController* PC = T.Controller()) PC->bShowDebug = false; });

	// ---- mob line-ups on a flat stage
	auto Lineup = [this, &Add](const TCHAR* Name, TArray<FName> Mobs, double Distance, double Spacing, float Pitch)
	{
		Add(Name, 2.f, true, [Mobs, Distance, Spacing, Pitch](AMCValidationTour& T)
		{
			T.ClearSpawned();
			T.Place(T.Stand, 0.f, Pitch);
			const int32 Made = T.SpawnLineup(Mobs, Distance, Spacing);
			T.Log(FString::Printf(TEXT("    spawned %d/%d"), Made, Mobs.Num()));
			return Made > 0;
		});
	};
	Add(TEXT("05_stage"), 2.f, true, [](AMCValidationTour& T)
	{
		T.Cmd(TEXT("time set 2500"));
		T.BuildStage();
		T.Place(T.Stand, 0.f, -12.f);
		return true;
	});
	Lineup(TEXT("06_mobs_hostile"), { TEXT("zombie"), TEXT("skeleton"), TEXT("creeper"), TEXT("spider"), TEXT("enderman"), TEXT("witch") }, 7.0, 1.9, -10.f);
	Lineup(TEXT("07_mobs_passive"), { TEXT("pig"), TEXT("cow"), TEXT("sheep"), TEXT("chicken"), TEXT("wolf"), TEXT("villager") }, 7.0, 1.9, -12.f);
	Lineup(TEXT("08_mobs_new"), { TEXT("sulfur_cube"), TEXT("copper_golem"), TEXT("armadillo"), TEXT("breeze"), TEXT("creaking"), TEXT("bogged") }, 7.0, 1.9, -10.f);
	Lineup(TEXT("09_mobs_nether"), { TEXT("piglin"), TEXT("zombified_piglin"), TEXT("blaze"), TEXT("wither_skeleton"), TEXT("magma_cube"), TEXT("shulker") }, 7.0, 1.9, -8.f);
	Lineup(TEXT("10_mobs_large"), { TEXT("iron_golem"), TEXT("camel"), TEXT("sniffer"), TEXT("ravager"), TEXT("warden") }, 11.0, 3.3, -6.f);
	Lineup(TEXT("11_mobs_small"), { TEXT("axolotl"), TEXT("frog"), TEXT("fox"), TEXT("bee"), TEXT("parrot"), TEXT("allay"), TEXT("rabbit") }, 5.0, 1.2, -18.f);
	Add(TEXT("12_entities"), 2.f, false, [](AMCValidationTour& T)
	{
		T.ClearSpawned();
		T.Place(T.Stand, 0.f, -14.f);
		const double X = FMath::FloorToDouble(T.Stand.X) + 6.5, Y = FMath::FloorToDouble(T.Stand.Y) + 0.5;
		const int32 Z = T.Floor + 1;
		// Minecraft order: x, y (height), z
		T.Cmd(FString::Printf(TEXT("summon oak_boat %.1f %d %.1f"), X, Z, Y - 3.0));
		T.Cmd(FString::Printf(TEXT("summon minecart %.1f %d %.1f"), X, Z, Y));
		T.Cmd(FString::Printf(TEXT("summon end_crystal %.1f %d %.1f"), X + 1.0, Z, Y + 3.0));
		T.Cmd(FString::Printf(TEXT("setblock %d %d %d rail"), FMath::FloorToInt(X), Z, FMath::FloorToInt(Y)));
		return true;
	});

	// ---- player views
	Add(TEXT("13_third_person_back"), 1.5f, false, [](AMCValidationTour& T)
	{
		T.LogCensus();
		T.ClearSpawned();
		if (AMCPlayer* P = T.Player()) { P->SelectSlot(6); P->CameraMode = 1; }
		T.Place(T.Stand + FVector(3, 0, 0), 20.f, -18.f);
		return true;
	});
	Add(TEXT("14_third_person_front"), 1.5f, false, [](AMCValidationTour& T)
	{
		if (AMCPlayer* P = T.Player()) P->CameraMode = 2;
		T.Place(T.Stand + FVector(3, 0, 0), 200.f, -10.f);
		return true;
	}, [](AMCValidationTour& T) { if (AMCPlayer* P = T.Player()) P->CameraMode = 0; });
	Add(TEXT("15_first_person_tool"), 1.5f, false, [](AMCValidationTour& T)
	{
		if (AMCPlayer* P = T.Player()) P->SelectSlot(6);
		T.Place(T.Stand, 0.f, -20.f);
		return true;
	});
	Add(TEXT("16_first_person_block"), 1.5f, false, [](AMCValidationTour& T)
	{
		if (AMCPlayer* P = T.Player()) P->SelectSlot(0);
		return true;
	});

	// ---- screens
	Add(TEXT("17_creative_inventory"), 1.5f, false, [](AMCValidationTour& T)
	{
		AMCPlayer* P = T.Player();
		if (!P) return false;
		P->OpenInventory();
		if (AMCPlayerController* PC = T.Controller()) PC->UpdateInputMode();
		return P->IsMenuOpen();
	}, [](AMCValidationTour& T) { T.CloseScreens(); });
	Add(TEXT("18_survival_inventory"), 1.5f, false, [Fly](AMCValidationTour& T)
	{
		T.Cmd(TEXT("gamemode survival"));
		Fly(T, false);
		T.Place(T.Stand, 0.f, -8.f);
		AMCPlayer* P = T.Player();
		if (!P) return false;
		P->OpenInventory();
		if (AMCPlayerController* PC = T.Controller()) PC->UpdateInputMode();
		return P->IsMenuOpen();
	}, [](AMCValidationTour& T) { T.CloseScreens(); });
	Add(TEXT("19_survival_hud"), 1.5f, false, [](AMCValidationTour& T)
	{
		T.Cmd(TEXT("xp add @s 7 levels"));
		T.Cmd(TEXT("give @s bread 12"));
		if (AMCPlayer* P = T.Player()) { P->Health = 13.f; P->FoodLevel = 15; }
		T.Place(T.Stand, 0.f, -8.f);
		return true;
	});
	auto Station = [&Add](const TCHAR* Name, const TCHAR* Block)
	{
		const FString BlockId = Block;
		Add(Name, 1.5f, false, [BlockId](AMCValidationTour& T)
		{
			AMCPlayer* P = T.Player();
			if (!P) return false;
			const FMCBlockPos B(FMath::FloorToInt(T.Stand.X) + 2, FMath::FloorToInt(T.Stand.Y), T.Floor + 1);
			T.Cmd(FString::Printf(TEXT("setblock %d %d %d %s"), B.X, B.Z, B.Y, *BlockId));
			// what a right click does: the block's use behaviour (stations), else its block entity (containers)
			bool bOpen = false;
			if (FMCWorld* W = P->World)
			{
				const FMCState St = W->GetState(B);
				const FMCBlock& Blk = FMCBlocks::GetByState(St);
				if (Blk.Behavior) bOpen = Blk.Behavior->OnUse(*W, B, St, P, EMCFace::West, FVector(0.f, 0.5f, 0.5f));
			}
			if (!P->IsMenuOpen()) bOpen = P->OpenBlockContainer(B);
			if (AMCPlayerController* PC = T.Controller()) PC->UpdateInputMode();
			return bOpen && P->IsMenuOpen();
		}, [](AMCValidationTour& T)
		{
			T.CloseScreens();
			T.Cmd(FString::Printf(TEXT("setblock %d %d %d air"), FMath::FloorToInt(T.Stand.X) + 2, T.Floor + 1, FMath::FloorToInt(T.Stand.Y)));
		});
	};
	Station(TEXT("20_crafting_table"), TEXT("crafting_table"));
	Station(TEXT("21_furnace"), TEXT("furnace"));
	Station(TEXT("22_chest"), TEXT("chest"));
	Station(TEXT("23_enchanting_table"), TEXT("enchanting_table"));
	Station(TEXT("24_brewing_stand"), TEXT("brewing_stand"));
	Station(TEXT("25_anvil"), TEXT("anvil"));

	// ---- lighting and weather
	Add(TEXT("26_night_lighting"), 3.f, true, [Fly](AMCValidationTour& T)
	{
		T.Cmd(TEXT("gamemode creative"));
		Fly(T, true);
		T.Cmd(TEXT("time set 18000"));
		const int32 X = FMath::FloorToInt(T.Stand.X), Y = FMath::FloorToInt(T.Stand.Y), Z = T.Floor + 1;
		const TCHAR* Lights[] = { TEXT("torch"), TEXT("lantern"), TEXT("glowstone"), TEXT("campfire"), TEXT("jack_o_lantern"), TEXT("soul_torch"), TEXT("sea_lantern"), TEXT("redstone_torch") };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Lights); ++i)
		{
			const int32 LX = X + 4 + (i % 4) * 3, LY = Y - 6 + (i / 4) * 10;
			T.Cmd(FString::Printf(TEXT("setblock %d %d %d %s"), LX, Z, LY, Lights[i]));
		}
		T.Place(T.Stand, 0.f, -14.f);
		return true;
	});
	Add(TEXT("27_rain"), 3.f, false, [](AMCValidationTour& T)
	{
		T.Cmd(TEXT("time set 5000"));
		T.Cmd(TEXT("weather rain"));
		T.Place(T.Stand, 40.f, 2.f);
		return true;
	}, [](AMCValidationTour& T) { T.Cmd(TEXT("weather clear")); });

	// ---- gameplay checks on the stage
	Add(TEXT("27a_tnt"), 5.5f, false, [Fly](AMCValidationTour& T)
	{
		T.Cmd(TEXT("time set 5000"));
		Fly(T, true);
		const int32 X = FMath::FloorToInt(T.Stand.X) + 9, Y = FMath::FloorToInt(T.Stand.Y) + 5, Z = T.Floor;
		T.TntPos = FIntVector(X, Y, Z + 1);
		// solid ground block around the charge, then a redstone block on top powers (primes) it like a lever would
		T.Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d dirt"), X - 3, Z - 2, Y - 3, X + 3, Z, Y + 3));
		T.Cmd(FString::Printf(TEXT("setblock %d %d %d tnt"), X, Z + 1, Y));
		T.Cmd(FString::Printf(TEXT("setblock %d %d %d redstone_block"), X, Z + 2, Y));
		T.Place(T.Stand + FVector(0, 0, 4), 0.f, -20.f);
		T.LookAt(FVector(X + 0.5, Y + 0.5, Z));
		return true;
	}, [](AMCValidationTour& T)
	{
		const FIntVector& C = T.TntPos;
		const int32 Removed = T.CountBlocks(FIntVector(C.X - 3, C.Y - 3, C.Z - 3), FIntVector(C.X + 3, C.Y + 3, C.Z - 1), true);
		const bool bTntGone = T.CountBlocks(C, C, false, FMCBlocks::C.TNTId) == 0;
		T.Log(FString::Printf(TEXT("    tnt: %s, crater %d of 147 ground blocks removed"), bTntGone ? TEXT("detonated") : TEXT("STILL PLACED"), Removed));
	});
	Add(TEXT("27b_nether_portal"), 2.5f, false, [](AMCValidationTour& T)
	{
		AMCGame* G = T.Game();
		FMCWorld* W = G ? G->ActiveWorld() : nullptr;
		if (!W) return false;
		const int32 X = FMath::FloorToInt(T.Stand.X) + 6, Y = FMath::FloorToInt(T.Stand.Y) - 7, Z = T.Floor + 1;
		T.PortalBase = FIntVector(X, Y, Z);
		// 4 x 5 obsidian frame along X, lit like flint and steel does (fire inside the frame)
		T.Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d obsidian"), X, Z, Y, X + 3, Z + 4, Y));
		T.Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d air"), X + 1, Z + 1, Y, X + 2, Z + 3, Y));
		const bool bLit = G->TryCreateNetherPortal(*W, FMCBlockPos(X + 1, Y, Z + 1));
		const int32 Portal = T.CountBlocks(FIntVector(X + 1, Y, Z + 1), FIntVector(X + 2, Y, Z + 3), false, FMCBlocks::C.NetherPortalId);
		T.Log(FString::Printf(TEXT("    nether portal: ignite=%s, %d/6 portal blocks"), bLit ? TEXT("ok") : TEXT("FAILED"), Portal));
		T.Place(FVector(X + 2.0, Y + 6.5, Z), 180.f, 0.f);
		T.LookAt(FVector(X + 2.0, Y + 0.5, Z + 2.5));
		return bLit && Portal == 6;
	});

	// ---- structures and biomes
	Add(TEXT("28_village"), 3.f, true, [](AMCValidationTour& T)
	{
		T.LogCensus();
		AMCGame* G = T.Game();
		FMCWorld* W = G ? G->ActiveWorld() : nullptr;
		AMCPlayer* P = T.Player();
		if (!W || !W->Generator || !P) return false;
		T.Cmd(TEXT("time set 2500"));
		TArray<FName> Names;
		W->Generator->GetStructureNames(Names);
		FMCBlockPos Best;
		double BestD = DBL_MAX;
		for (FName N : Names)
		{
			if (!N.ToString().Contains(TEXT("village"))) continue;
			FMCBlockPos Pos;
			if (!W->Generator->LocateStructure(N, P->BlockPos(), 100, Pos)) continue;
			const double D = FVector2D::Distance(FVector2D(Pos.X, Pos.Y), FVector2D(P->Pos.X, P->Pos.Y));
			if (D < BestD) { BestD = D; Best = Pos; }
		}
		if (BestD == DBL_MAX) { T.Log(TEXT("    no village located")); return false; }
		W->ForceLoadArea(FMCChunkPos::FromBlock(Best.X, Best.Y), 3, 30.0);
		const int32 Ground = W->GetHeight(Best.X - 22, Best.Y - 22);
		T.Place(FVector(Best.X - 22 + 0.5, Best.Y - 22 + 0.5, Ground + 14), 45.f, -20.f);
		T.LookAt(FVector(Best.X + 0.5, Best.Y + 0.5, W->GetHeight(Best.X, Best.Y) + 2));
		T.Log(FString::Printf(TEXT("    village at %d %d (%.0f blocks)"), Best.X, Best.Y, BestD));
		return true;
	});
	Add(TEXT("29_sulfur_caves"), 3.f, true, [](AMCValidationTour& T)
	{
		FVector Pocket;
		if (!T.FindCavePocket(TEXT("sulfur_caves"), Pocket)) { T.Log(TEXT("    no sulfur caves pocket found")); return false; }
		T.Cmd(TEXT("effect give @s night_vision 120 0 true"));
		T.Place(Pocket, 0.f, -8.f);
		T.Log(FString::Printf(TEXT("    sulfur caves pocket at %.0f %.0f %.0f"), Pocket.X, Pocket.Y, Pocket.Z));
		// look along the most open horizontal direction
		const AMCGame* G = T.Game();
		const FMCWorld* W = G ? G->ActiveWorld() : nullptr;
		if (W)
		{
			float BestYaw = 0.f; int32 BestRun = -1;
			for (int32 a = 0; a < 8; ++a)
			{
				const float Yaw = a * 45.f;
				const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
				int32 Run = 0;
				for (int32 s = 1; s < 24; ++s)
				{
					const FVector Q = Pocket + FVector(0, 0, 1.5) + Dir * s;
					if (!W->IsAir(FMCBlockPos(MC::FloorToInt(Q.X), MC::FloorToInt(Q.Y), MC::FloorToInt(Q.Z)))) break;
					++Run;
				}
				if (Run > BestRun) { BestRun = Run; BestYaw = Yaw; }
			}
			T.Place(Pocket, BestYaw, -8.f);
		}
		return true;
	}, [](AMCValidationTour& T) { T.Cmd(TEXT("effect clear @s")); });
	Add(TEXT("30_lush_caves"), 3.f, true, [](AMCValidationTour& T)
	{
		FVector Pocket;
		if (!T.FindCavePocket(TEXT("lush_caves"), Pocket)) { T.Log(TEXT("    no lush caves pocket found")); return false; }
		T.Cmd(TEXT("effect give @s night_vision 120 0 true"));
		T.Place(Pocket, 45.f, -6.f);
		return true;
	}, [](AMCValidationTour& T) { T.Cmd(TEXT("effect clear @s")); });

	// ---- other dimensions
	Add(TEXT("31_nether"), 3.f, true, [](AMCValidationTour& T)
	{
		T.LogCensus();
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P) return false;
		G->ChangeDimension(P, EMCDimension::Nether);
		// hover beside the arrival portal so its trigger cannot send the player back during the shot
		P->bFlying = P->bMayFly;
		P->TeleportTo(P->Pos + FVector(3.0, 0.0, 2.0));
		P->PortalCooldown = 100000;
		P->ViewPitch = P->Pitch = -4.f;
		return G->ActiveDim == EMCDimension::Nether;
	});
	Add(TEXT("32_nether_reverse"), 2.f, true, [](AMCValidationTour& T)
	{
		AMCPlayer* P = T.Player();
		if (!P || !T.Game() || T.Game()->ActiveDim != EMCDimension::Nether) return false;
		P->bFlying = P->bMayFly;
		P->PortalCooldown = 100000;
		P->TeleportTo(P->Pos + FVector(0, 0, 6));
		P->ViewYaw = P->Yaw = P->Yaw + 180.f;
		P->ViewPitch = P->Pitch = -12.f;
		return true;
	});
	Add(TEXT("33_end"), 3.f, true, [](AMCValidationTour& T)
	{
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P) return false;
		G->ChangeDimension(P, EMCDimension::End);
		P->bFlying = P->bMayFly;
		T.LookAt(FVector(0, 0, 72));
		return G->ActiveDim == EMCDimension::End;
	});
	Add(TEXT("34_end_island"), 3.f, true, [](AMCValidationTour& T)
	{
		AMCPlayer* P = T.Player();
		if (!P) return false;
		P->bFlying = P->bMayFly;
		T.Place(FVector(38.5, 30.5, 96), 0.f, 0.f);
		T.LookAt(FVector(0, 0, 70));
		// the dragon, if it is up, becomes the subject
		if (const AMCGame* G = T.Game()) if (AMCEnderDragon* D = G->DragonFight.Dragon.Get()) { T.LookAt(D->Pos + FVector(0, 0, 2)); T.Log(TEXT("    dragon present")); }
		return true;
	});
	Add(TEXT("35_portal_return"), 2.f, true, [](AMCValidationTour& T)
	{
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P || T.PortalBase == FIntVector::ZeroValue) return false;
		G->ChangeDimension(P, EMCDimension::Overworld);
		P->bFlying = P->bMayFly;
		const FIntVector& B = T.PortalBase;
		T.Place(FVector(B.X + 2.0, B.Y + 6.5, B.Z), 180.f, 0.f);
		T.LookAt(FVector(B.X + 2.0, B.Y + 0.5, B.Z + 2.5));
		return G->ActiveDim == EMCDimension::Overworld;
	}, [](AMCValidationTour& T)
	{
		const FIntVector& B = T.PortalBase;
		const int32 Portal = T.CountBlocks(FIntVector(B.X + 1, B.Y, B.Z + 1), FIntVector(B.X + 2, B.Y, B.Z + 3), false, FMCBlocks::C.NetherPortalId);
		T.Log(FString::Printf(TEXT("    portal still lit after the round trip: %d/6 blocks"), Portal));
	});
	Add(TEXT("36_portal_travel"), 3.f, true, [](AMCValidationTour& T)
	{
		AMCPlayer* P = T.Player();
		if (!P || T.PortalBase == FIntVector::ZeroValue) return false;
		const FMCItemStack& H = P->HeldConst();
		T.HeldBeforePortal = H.IsEmpty() ? TEXT("-") : FString::Printf(TEXT("%s x%d"), *H.Item().Name.ToString(), H.Count);
		// walk into the portal: the creative player crosses after one tick inside, the arrival is streamed and shot
		P->bFlying = false;
		P->PortalCooldown = 0;
		P->TeleportTo(FVector(T.PortalBase.X + 2.0, T.PortalBase.Y + 0.5, T.PortalBase.Z + 1.0));
		return true;
	}, [](AMCValidationTour& T)
	{
		const AMCGame* G = T.Game();
		const AMCPlayer* P = T.Player();
		if (!G || !P) return;
		const FMCItemStack& H = P->HeldConst();
		const FString After = H.IsEmpty() ? TEXT("-") : FString::Printf(TEXT("%s x%d"), *H.Item().Name.ToString(), H.Count);
		T.Log(FString::Printf(TEXT("    portal travel: dim=%d (%s) pos=(%.1f, %.1f, %.1f), held before=%s after=%s"), (int32)G->ActiveDim,
			G->ActiveDim == EMCDimension::Nether ? TEXT("arrived in the Nether") : TEXT("DID NOT TRAVEL"), P->Pos.X, P->Pos.Y, P->Pos.Z, *T.HeldBeforePortal, *After));
	});
	Add(TEXT("37_end_portal_site"), 1.5f, true, [](AMCValidationTour& T)
	{
		// back to the Overworld and over to the stage; the building happens once its chunks are streamed in
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P) return false;
		if (G->ActiveDim != EMCDimension::Overworld) G->ChangeDimension(P, EMCDimension::Overworld);
		P->bFlying = P->bMayFly;
		T.Place(T.Stand + FVector(-6, 2.5, 3), 90.f, -25.f);
		return G->ActiveDim == EMCDimension::Overworld;
	});
	Add(TEXT("37b_end_portal"), 2.5f, false, [](AMCValidationTour& T)
	{
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P || G->ActiveDim != EMCDimension::Overworld) return false;
		FMCWorld* W = G->ActiveWorld();
		const FMCBlock* PortalBlock = FMCBlocks::Find(TEXT("end_portal"));
		if (!W || !PortalBlock) return false;
		// a ring of 12 frames standing on flat ground (no pit under it), eyes inserted the way a player does it
		const int32 CX = FMath::FloorToInt(T.Stand.X) - 6, CY = FMath::FloorToInt(T.Stand.Y) + 9, Z = T.Floor + 1;
		T.EndPortalCenter = FIntVector(CX, CY, Z);
		T.Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d grass_block"), CX - 3, T.Floor, CY - 3, CX + 3, T.Floor, CY + 3));
		T.Cmd(FString::Printf(TEXT("fill %d %d %d %d %d %d air"), CX - 3, Z, CY - 3, CX + 3, Z + 3, CY + 3));
		TArray<FMCBlockPos> Frames;
		for (int32 i = -1; i <= 1; ++i)
		{
			Frames.Add(FMCBlockPos(CX + i, CY - 2, Z)); Frames.Add(FMCBlockPos(CX + i, CY + 2, Z));
			Frames.Add(FMCBlockPos(CX - 2, CY + i, Z)); Frames.Add(FMCBlockPos(CX + 2, CY + i, Z));
		}
		for (const FMCBlockPos& F : Frames) T.Cmd(FString::Printf(TEXT("setblock %d %d %d end_portal_frame"), F.X, F.Z, F.Y));
		P->Held() = FMCItemStack::Of(TEXT("ender_eye"), 16);
		for (const FMCBlockPos& F : Frames)
		{
			const FMCState St = W->GetState(F);
			const FMCBlock& Blk = FMCBlocks::GetByState(St);
			if (Blk.Behavior) Blk.Behavior->OnUse(*W, F, St, P, EMCFace::Up, FVector(0.5f, 0.5f, 1.f));
		}
		const int32 Portal = T.CountBlocks(FIntVector(CX - 1, CY - 1, Z), FIntVector(CX + 1, CY + 1, Z), false, PortalBlock->Id);
		T.Log(FString::Printf(TEXT("    end portal: 12 frames on flat ground + eyes -> %d/9 portal blocks at frame height"), Portal));
		P->bFlying = P->bMayFly;
		T.Place(FVector(CX + 0.5, CY - 6.5, Z + 3), 90.f, -25.f);
		T.LookAt(FVector(CX + 0.5, CY + 0.5, Z));
		return Portal == 9;
	});
	Add(TEXT("38_end_portal_travel"), 3.f, true, [](AMCValidationTour& T)
	{
		AMCPlayer* P = T.Player();
		if (!P || T.EndPortalCenter == FIntVector::ZeroValue) return false;
		// step onto an edge block of the 3x3 (not the centre): any portal block must work
		P->bFlying = false;
		P->PortalCooldown = 0;
		const FIntVector& C = T.EndPortalCenter;
		P->TeleportTo(FVector(C.X + 1.5, C.Y + 0.5, C.Z + 0.1));
		return true;
	}, [](AMCValidationTour& T)
	{
		const AMCGame* G = T.Game();
		const bool bEnd = G && G->ActiveDim == EMCDimension::End;
		T.Log(FString::Printf(TEXT("    end portal travel: dim=%d (%s)"), G ? (int32)G->ActiveDim : -1, bEnd ? TEXT("arrived in the End") : TEXT("DID NOT TRAVEL")));
	});
	Add(TEXT("39_end_exit_portal"), 3.f, true, [](AMCValidationTour& T)
	{
		AMCGame* G = T.Game();
		AMCPlayer* P = T.Player();
		if (!G || !P || G->ActiveDim != EMCDimension::End) return false;
		FMCWorld* W = G->ActiveWorld();
		const FMCEndGen* EG = W ? static_cast<const FMCEndGen*>(W->Generator.Get()) : nullptr;
		if (!EG) return false;
		// the exit portal opens when the dragon dies; open it directly and step into its ring
		G->SpawnExitPortal(true);
		P->bFlying = false;
		P->PortalCooldown = 0;
		P->TeleportTo(FVector(2.5, 0.5, EG->ExitPortalZ() + 0.1));
		return true;
	}, [](AMCValidationTour& T)
	{
		const AMCGame* G = T.Game();
		const bool bHome = G && G->ActiveDim == EMCDimension::Overworld;
		T.Log(FString::Printf(TEXT("    end exit portal: dim=%d (%s)"), G ? (int32)G->ActiveDim : -1, bHome ? TEXT("back in the Overworld") : TEXT("DID NOT TRAVEL")));
	});
}

void AMCValidationTour::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) return;
	if (FParse::Param(FCommandLine::Get(), TEXT("Opus55TitleShot")))
	{
		TotalTime += DeltaSeconds;
		const AMCGame* TG = Game();
		if (ShotTime < 0.0 && TotalTime > 12.0 && TG && TG->bTitleScreen)
		{
			const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Tour"));
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, TEXT("00_title_screen.png")), true, false);
			ShotTime = TotalTime;
		}
		// then create a world the way the "Create New World" button does and check the switch to the game
		if (ShotTime >= 0.0 && WarmupTime == 0.0 && TotalTime > ShotTime + 1.5)
		{
			if (AMCGame* SG = Game()) SG->StartWorld(TEXT("Title Flow Check"), 4242, false, EMCGameMode::Creative);
			WarmupTime = TotalTime;
		}
		const AMCGame* WG = Game();
		if (WarmupTime > 0.0 && WG && WG->bWorldReady && !WG->bTitleScreen && !WG->bLoading && WG->Player && !IsStreaming() && StepTime == 0.0)
			StepTime = TotalTime;
		if (StepTime > 0.0 && TotalTime > StepTime + 3.0 && Shots == 0)
		{
			const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Tour"));
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, TEXT("00b_world_from_title.png")), true, false);
			const AMCPlayerController* PC = Controller();
			UE_LOG(LogOpus55, Display, TEXT("OPUS55_TITLE world started from the title: player=%d cursor=%d uiMouse=%d mode=%d after %.1fs"),
				WG->Player ? 1 : 0, PC && PC->bShowMouseCursor ? 1 : 0, PC && PC->bUIWantsMouse ? 1 : 0, WG->Player ? (int32)WG->Player->GameMode : -1, TotalTime - WarmupTime);
			Shots = 1;
			ShotTime = TotalTime;
		}
		if (Shots == 1 && TotalTime > ShotTime + 1.5)
		{
			bFinished = true;
			FPlatformMisc::RequestExit(false, TEXT("Opus55TitleShot"));
		}
		if (WarmupTime > 0.0 && TotalTime > WarmupTime + 180.0)
		{
			UE_LOG(LogOpus55, Warning, TEXT("OPUS55_TITLE world did not become ready from the title screen"));
			bFinished = true;
			FPlatformMisc::RequestExit(false, TEXT("Opus55TitleShot"));
		}
		return;
	}
	TotalTime += DeltaSeconds;
	AMCGame* G = Game();
	AMCPlayer* P = Player();

	// warm-up: world running, terrain around the spawn meshed
	if (Current < 0)
	{
		if (!G || !G->bWorldReady || !P || G->bTitleScreen) return;
		if (WarmupTime == 0.0)
		{
			// fix the clock and weather first: the sky, Lumen and the exposure need a few seconds to settle after a jump
			Cmd(TEXT("gamerule doDaylightCycle false"));
			Cmd(TEXT("gamerule doWeatherCycle false"));
			Cmd(TEXT("weather clear"));
			Cmd(TEXT("time set 2500"));
		}
		WarmupTime += DeltaSeconds;
		if ((IsStreaming() && WarmupTime < 150.0) || WarmupTime < 8.0) return;
		OutDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Tour"));
		IFileManager::Get().MakeDirectory(*OutDir, true);
		Stand = P->Pos;
		Floor = FMath::FloorToInt(P->Pos.Z) - 1;
		Log(FString::Printf(TEXT("warm-up %.1fs, world '%s' seed %llu, stand (%.1f, %.1f, %.1f)"), WarmupTime, *G->WorldName, G->Seed, Stand.X, Stand.Y, Stand.Z));
		Log(TEXT("  ") + Stats());
		LogCensus();
		BuildSteps();
		Current = 0;
		StepTime = 0.0;
		ShotTime = -1.0;
		bSkipped = !Steps[0].Enter(*this);
		return;
	}

	// frame statistics (only while a scene is being held, not while streaming a new area)
	if (!IsStreaming())
	{
		FrameSum += DeltaSeconds;
		WorstFrame = FMath::Max(WorstFrame, (double)DeltaSeconds);
		++Frames;
	}

	FStep& S = Steps[Current];
	StepTime += DeltaSeconds;
	bool bAdvance = false;
	if (bSkipped)
	{
		Log(FString::Printf(TEXT("SKIP %s"), *S.Name));
		++Skips;
		bAdvance = true;
	}
	else if (ShotTime < 0.0)
	{
		const bool bReady = StepTime >= S.Settle && (!S.bWaitStreaming || !IsStreaming() || StepTime >= S.MaxWait);
		if (bReady)
		{
			const FString Path = FPaths::Combine(OutDir, S.Name + TEXT(".png"));
			FScreenshotRequest::RequestScreenshot(Path, S.bShowUI, false);
			ShotTime = StepTime;
			Log(FString::Printf(TEXT("SHOT %s after %.1fs%s  %s"), *S.Name, StepTime, (S.bWaitStreaming && IsStreaming()) ? TEXT(" (streaming timeout)") : TEXT(""), *Stats()));
			++Shots;
			// component dumps for the scenes whose subjects are runtime-built visuals
			if (S.Name.Contains(TEXT("mobs_hostile")) && Spawned.Num() > 0) Log(TEXT("    first mob: ") + DescribeComponents(Spawned[0].Get(), 4));
			if (S.Name.Contains(TEXT("first_person_tool"))) Log(TEXT("    player: ") + DescribeComponents(Player(), 12));
		}
	}
	else if (StepTime - ShotTime > 0.6)
	{
		bAdvance = true;
	}

	if (bAdvance)
	{
		if (!bSkipped && S.Leave) S.Leave(*this);
		++Current;
		if (Current >= Steps.Num()) { Finish(); return; }
		StepTime = 0.0;
		ShotTime = -1.0;
		bSkipped = !Steps[Current].Enter(*this);
	}
}

void AMCValidationTour::LogCensus()
{
	// entity census by type (spawn-rule sanity check)
	const AMCGame* G = Game();
	const FMCWorld* W = G ? G->ActiveWorld() : nullptr;
	if (!W) return;
	TMap<FString, int32> Census;
	for (const AMCEntity* E : W->Entities)
	{
		if (!E) continue;
		const AMCMob* M = Cast<AMCMob>(E);
		FString Type = (M && M->Def) ? M->Def->Id.ToString() : E->GetClass()->GetName();
		if (const AMCItemEntity* I = Cast<AMCItemEntity>(E)) Type = TEXT("item:") + (I->Stack.IsEmpty() ? FString(TEXT("empty")) : I->Stack.Item().Name.ToString());
		Census.FindOrAdd(Type)++;
	}
	Census.ValueSort([](int32 A, int32 B) { return A > B; });
	FString Line = FString::Printf(TEXT("  entities by type (%d):"), W->Entities.Num());
	int32 Shown = 0;
	for (const TPair<FString, int32>& Pair : Census) { if (Shown++ >= 16) break; Line += FString::Printf(TEXT(" %s=%d"), *Pair.Key, Pair.Value); }
	Log(Line);
}

void AMCValidationTour::Finish()
{
	LogCensus();
	bFinished = true;
	const double AvgMs = Frames > 0 ? FrameSum / Frames * 1000.0 : 0.0;
	Log(FString::Printf(TEXT("done: %d screenshots, %d skipped, %.1fs total, average frame %.2f ms (%.0f fps), worst frame %.1f ms"),
		Shots, Skips, TotalTime, AvgMs, AvgMs > 0.0 ? 1000.0 / AvgMs : 0.0, WorstFrame * 1000.0));
	if (AMCGame* G = Game())
	{
		if (!bLoadCheck)
		{
			G->SaveWorld(true);
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Worlds"), G->WorldName), TEXT("*"), true, false);
			Log(FString::Printf(TEXT("saved world '%s': %d files"), *G->WorldName, Files.Num()));
		}
	}
	FFileHelper::SaveStringArrayToFile(Report, *FPaths::Combine(OutDir, bLoadCheck ? TEXT("load_report.txt") : TEXT("tour_report.txt")));
	if (!FParse::Param(FCommandLine::Get(), TEXT("Opus55TourStay"))) FPlatformMisc::RequestExit(false, TEXT("Opus55Tour"));
}
