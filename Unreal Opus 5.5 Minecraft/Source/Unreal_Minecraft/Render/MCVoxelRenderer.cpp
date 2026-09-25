#include "Render/MCVoxelRenderer.h"
#include "Render/MCChunkMeshComponent.h"
#include "Render/MCTextureSynth.h"
#include "Render/MCIcons.h"
#include "Render/MCAssets.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2DArray.h"
#include "Tasks/Task.h"
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCVoxelRenderer)

namespace
{
	const TCHAR* GLayerNames[FMCChunkMeshData::NumLayers] = { TEXT("Opaque"), TEXT("Cutout"), TEXT("Translucent"), TEXT("Water"), TEXT("Lava"), TEXT("Portal"), TEXT("EndPortal") };
	constexpr int32 MaxPoolLights = 24;
	constexpr double LightRangeBlocks = 22.0;

	FLinearColor EmitterColor(const FMCBlock& B)
	{
		const FString N = B.Name.ToString();
		if (N.Contains(TEXT("soul"))) return FLinearColor(0.35f, 0.75f, 1.f);
		if (N.Contains(TEXT("redstone")) || N == TEXT("repeater") || N == TEXT("comparator")) return FLinearColor(1.f, 0.25f, 0.15f);
		if (N.Contains(TEXT("end_rod")) || N.Contains(TEXT("froglight")) || N.Contains(TEXT("beacon"))) return FLinearColor(0.95f, 0.95f, 1.f);
		if (N.Contains(TEXT("sea_lantern")) || N.Contains(TEXT("conduit")) || N.Contains(TEXT("glow_lichen")) || N.Contains(TEXT("sculk"))) return FLinearColor(0.6f, 0.95f, 1.f);
		if (N.Contains(TEXT("amethyst")) || N.Contains(TEXT("portal")) || N.Contains(TEXT("crying"))) return FLinearColor(0.75f, 0.45f, 1.f);
		if (N.Contains(TEXT("lava")) || N.Contains(TEXT("magma")) || N.Contains(TEXT("fire")) || N.Contains(TEXT("campfire"))) return FLinearColor(1.f, 0.5f, 0.18f);
		if (N.Contains(TEXT("sulfur"))) return FLinearColor(1.f, 0.9f, 0.35f);
		return FLinearColor(1.f, 0.72f, 0.42f); // torches, lanterns, glowstone, lamps
	}

	/** Inverse of AMCVoxelRenderer::MakeKey (Y is stored as a sign-extended 25-bit field). */
	FMCChunkPos KeyChunk(int64 Key)
	{
		int32 CY = (int32)(((uint64)Key >> 3) & 0x1FFFFFF);
		if (CY & 0x1000000) CY |= ~0x1FFFFFF;
		return FMCChunkPos((int32)(Key >> 32), CY);
	}
}

AMCVoxelRenderer::AMCVoxelRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;
	Results = MakeShared<TQueue<TSharedPtr<FMCMeshResult>, EQueueMode::Mpsc>>();
	TasksRunning = MakeShared<FThreadSafeCounter>();
}

void AMCVoxelRenderer::EndPlay(const EEndPlayReason::Type Reason)
{
	// wait for workers so they never touch freed memory (they only reference shared state, but be tidy)
	const double Start = FPlatformTime::Seconds();
	while (TasksRunning->GetValue() > 0 && FPlatformTime::Seconds() - Start < 5.0) FPlatformProcess::Sleep(0.002f);
	if (MCRender::GVoxelMaterials == &LayerMaterials) MCRender::GVoxelMaterials = nullptr;
	if (MCRender::GItemMaterials == &ItemMaterials) MCRender::GItemMaterials = nullptr;
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Materials

void AMCVoxelRenderer::InitMaterials()
{
	if (bMaterialsReady) return;
	bMaterialsReady = true;
	const double T0 = FPlatformTime::Seconds();

	TArray<FColor> Albedo, Normal, Orme;
	MCTexSynth::BuildTerrain(Albedo, Normal, Orme);
	const int32 S = MCTexSynth::LayerSize, N = MCTexSynth::NumLayers();
	AlbedoArray = MCTexSynth::CreateArray(this, S, N, Albedo, true, false, true);
	NormalArray = MCTexSynth::CreateArray(this, S, N, Normal, false, true, false);
	OrmeArray = MCTexSynth::CreateArray(this, S, N, Orme, false, false, false);
	IconArray = MCIcons::CreateSpriteArray(this);

	// flat 1-layer normal / ORME arrays for item sprites
	TArray<FColor> FlatN; FlatN.Init(FColor(128, 128, 255, 255), 4 * 4);
	TArray<FColor> FlatO; FlatO.Init(FColor(255, 190, 0, 0), 4 * 4);
	UTexture2DArray* FlatNormal = MCTexSynth::CreateArray(this, 4, 1, FlatN, false, true, false);
	UTexture2DArray* FlatOrme = MCTexSynth::CreateArray(this, 4, 1, FlatO, false, false, false);

	LayerMaterials.Reset(); LayerMIDs.Reset(); ItemMaterials.Reset(); ItemMIDs.Reset();
	for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
	{
		const FString Path = FString::Printf(TEXT("/Game/Opus55Minecraft/Materials/M_MCVoxel_%s.M_MCVoxel_%s"), GLayerNames[L], GLayerNames[L]);
		UMaterialInterface* Base = MCAssets::Material(*Path);
		if (!Base)
		{
			UE_LOG(LogOpus55, Warning, TEXT("Voxel material %s missing - run the content build commandlet. Using the default material."), *Path);
			Base = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetTextureParameterValue(TEXT("Albedo"), AlbedoArray);
		MID->SetTextureParameterValue(TEXT("Normal"), NormalArray);
		MID->SetTextureParameterValue(TEXT("ORME"), OrmeArray);
		MID->SetScalarParameterValue(TEXT("LayerCount"), (float)N);
		LayerMIDs.Add(MID);
		LayerMaterials.Add(MID);

		UMaterialInstanceDynamic* IMID = UMaterialInstanceDynamic::Create(Base, this);
		const bool bSprite = L == (int32)EMCLayer::Cutout;
		IMID->SetTextureParameterValue(TEXT("Albedo"), bSprite && IconArray ? (UTexture*)IconArray : (UTexture*)AlbedoArray);
		IMID->SetTextureParameterValue(TEXT("Normal"), bSprite ? (UTexture*)FlatNormal : (UTexture*)NormalArray);
		IMID->SetTextureParameterValue(TEXT("ORME"), bSprite ? (UTexture*)FlatOrme : (UTexture*)OrmeArray);
		IMID->SetScalarParameterValue(TEXT("ItemMode"), 1.f);
		ItemMIDs.Add(IMID);
		ItemMaterials.Add(IMID);
	}
	MCRender::GVoxelMaterials = &LayerMaterials;
	MCRender::GItemMaterials = &ItemMaterials;

	// selection outline & crack overlay
	Outline = NewObject<UMCChunkMeshComponent>(this, TEXT("SelectionOutline"));
	Outline->SetMobility(EComponentMobility::Movable);
	Outline->SetCastShadow(false);
	Outline->SharedMaterials = &LayerMaterials;
	Outline->SetupAttachment(RootComponent);
	Outline->RegisterComponent();
	Crack = NewObject<UMCChunkMeshComponent>(this, TEXT("CrackOverlay"));
	Crack->SetMobility(EComponentMobility::Movable);
	Crack->SetCastShadow(false);
	Crack->SharedMaterials = &LayerMaterials;
	Crack->SetupAttachment(RootComponent);
	Crack->RegisterComponent();

	for (int32 i = 0; i < MaxPoolLights; ++i)
	{
		UPointLightComponent* PL = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("BlockLight%d"), i));
		PL->SetMobility(EComponentMobility::Movable);
		PL->SetCastShadows(false);
		PL->SetIntensityUnits(ELightUnits::Candelas);
		PL->SetIntensity(0.f);
		PL->SetVisibility(false);
		PL->bAffectsWorld = true;
		PL->SetLightingChannels(false, true, false); // entities opt into channel 1; terrain uses baked voxel light
		PL->SetupAttachment(RootComponent);
		PL->RegisterComponent();
		LightPool.Add(PL);
	}
	UE_LOG(LogOpus55, Log, TEXT("Voxel renderer materials ready: %d layers (%dx%d) in %.2fs"), N, S, S, FPlatformTime::Seconds() - T0);
}

void AMCVoxelRenderer::SetGlobalParams(float SkyBrightness, float AmbientFloor, float NightVision, float TimeSeconds, float Wetness, const FLinearColor& FogColor)
{
	auto Apply = [&](UMaterialInstanceDynamic* M)
	{
		if (!M) return;
		M->SetScalarParameterValue(TEXT("SkyBrightness"), SkyBrightness);
		M->SetScalarParameterValue(TEXT("AmbientFloor"), AmbientFloor);
		M->SetScalarParameterValue(TEXT("NightVision"), NightVision);
		M->SetScalarParameterValue(TEXT("GameTime"), FMath::Fmod(TimeSeconds, 3600.f));
		M->SetScalarParameterValue(TEXT("Wetness"), Wetness);
		M->SetVectorParameterValue(TEXT("FogColor"), FogColor);
	};
	for (UMaterialInstanceDynamic* M : LayerMIDs) Apply(M);
	for (UMaterialInstanceDynamic* M : ItemMIDs) Apply(M);
}

// ---------------------------------------------------------------------------------------------------------------------
// World binding

void AMCVoxelRenderer::SetWorld(FMCWorld* InWorld)
{
	InitMaterials();
	TArray<int64> Keys;
	Groups.GetKeys(Keys);
	for (int64 K : Keys) ReleaseGroup(K);
	GroupRevision.Reset();
	GroupEmitters.Reset();
	PendingChunks.Reset();
	InFlight.Reset();
	++WorldSerial;
	if (World && World->Renderer == this) World->Renderer = nullptr;
	World = InWorld;
	SelStage = -2;
	if (Outline) Outline->ClearMesh();
	if (Crack) Crack->ClearMesh();
	for (UPointLightComponent* PL : LightPool) { PL->SetVisibility(false); }
	if (World)
	{
		World->Renderer = this;
		for (auto& Pair : World->Chunks)
		{
			if (Pair.Value->Stage == EMCChunkStage::Lit) { Pair.Value->MarkAllDirty(); PendingChunks.Add(Pair.Key); }
		}
	}
}

void AMCVoxelRenderer::RebuildAll()
{
	if (!World) return;
	for (auto& Pair : World->Chunks) { Pair.Value->MarkAllDirty(); PendingChunks.Add(Pair.Key); }
}

UMCChunkMeshComponent* AMCVoxelRenderer::AcquireComponent(int64 Key, const FMCChunkPos& C, int32 Group)
{
	if (TObjectPtr<UMCChunkMeshComponent>* Found = Groups.Find(Key)) return Found->Get();
	UMCChunkMeshComponent* Comp = nullptr;
	const FVector Loc(C.X * 1600.0, C.Y * 1600.0, MCRender::GroupBaseZ(Group) * MC::BlockSize);
	if (FreeComponents.Num() > 0)
	{
		Comp = FreeComponents.Pop(EAllowShrinking::No);
		Comp->SetRelativeLocation(Loc);
		Comp->RegisterComponent();
	}
	else
	{
		Comp = NewObject<UMCChunkMeshComponent>(this);
		Comp->SharedMaterials = &LayerMaterials;
		Comp->SetupAttachment(RootComponent);
		Comp->SetRelativeLocation(Loc);
		Comp->RegisterComponent();
	}
	Groups.Add(Key, Comp);
	return Comp;
}

void AMCVoxelRenderer::ReleaseGroup(int64 Key)
{
	TObjectPtr<UMCChunkMeshComponent> Comp;
	if (Groups.RemoveAndCopyValue(Key, Comp) && Comp)
	{
		StatTriangles -= Comp->LastTriangles;
		Comp->ClearMesh();
		Comp->UnregisterComponent();
		if (FreeComponents.Num() < 256) FreeComponents.Add(Comp);
		else Comp->DestroyComponent();
	}
	GroupEmitters.Remove(Key);
}

void AMCVoxelRenderer::DropChunk(const FMCChunkPos& C)
{
	for (int32 G = 0; G < MCRender::NumGroups; ++G) ReleaseGroup(MakeKey(C, G));
	PendingChunks.Remove(C);
}

bool AMCVoxelRenderer::CanMesh(const FMCChunkPos& C) const
{
	const FMCChunk* Ch = World->GetChunk(C);
	if (!Ch || Ch->Stage != EMCChunkStage::Lit) return false;
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			if (!dx && !dy) continue;
			const FMCChunk* N = World->GetChunk(FMCChunkPos(C.X + dx, C.Y + dy));
			if (N && N->Stage != EMCChunkStage::Lit) return false; // neighbour arriving soon - wait for seamless borders
		}
	return true;
}

void AMCVoxelRenderer::LaunchMesh(const FMCChunkPos& C, int32 Group)
{
	const int64 Key = MakeKey(C, Group);
	TSharedPtr<FMCMeshInput> In = MakeShared<FMCMeshInput>();
	if (!In->Capture(*World, C, Group))
	{
		ReleaseGroup(Key);
		return;
	}
	const uint32 Rev = ++RevisionCounter;
	In->Revision = Rev;
	GroupRevision.Add(Key, Rev);
	InFlight.Add(Key);
	TasksRunning->Increment();
	TSharedPtr<TQueue<TSharedPtr<FMCMeshResult>, EQueueMode::Mpsc>> Q = Results;
	TSharedPtr<FThreadSafeCounter> Running = TasksRunning;
	const uint64 Serial = WorldSerial;
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [In, Q, Running, Key, Rev, Serial]()
	{
		TSharedPtr<FMCMeshResult> R = MakeShared<FMCMeshResult>();
		R->Key = Key;
		R->Revision = Rev;
		R->WorldSerial = Serial;
		R->Data = FMCMesher::Build(*In);
		// light emitters of this group (for the dynamic light pool)
		for (int32 Z = 0; Z < MCRender::GroupHeight; ++Z)
			for (int32 Y = 0; Y < 16; ++Y)
				for (int32 X = 0; X < 16; ++X)
				{
					const FMCState S = In->Get(X, Y, Z);
					if (S == 0) continue;
					const uint8 L = FMCBlocks::Info(S).Light;
					if (L >= 10) R->Emitters.Add(FIntVector4(In->Chunk.MinBlockX() + X, In->Chunk.MinBlockY() + Y, In->BaseZ + Z, L));
				}
		Q->Enqueue(R);
		Running->Decrement();
	}, LowLevelTasks::ETaskPriority::BackgroundNormal);
}

void AMCVoxelRenderer::ApplyResult(TSharedPtr<FMCMeshResult> R)
{
	InFlight.Remove(R->Key);
	if (R->WorldSerial != WorldSerial) return;
	const uint32* Latest = GroupRevision.Find(R->Key);
	if (!Latest || *Latest != R->Revision) return;
	const int32 Group = (int32)(R->Key & 7);
	const FMCChunkPos CP = KeyChunk(R->Key);
	if (!World || !World->GetChunk(CP)) return;
	if (!R->Data.IsValid() || R->Data->IsEmpty())
	{
		ReleaseGroup(R->Key);
		return;
	}
	StatTriangles += R->Data->NumTriangles();
	UMCChunkMeshComponent* Comp = AcquireComponent(R->Key, CP, Group);
	StatTriangles -= Comp->LastTriangles;
	Comp->SetMeshData(MoveTemp(R->Data));
	if (R->Emitters.Num() > 0) GroupEmitters.Add(R->Key, MoveTemp(R->Emitters));
	else GroupEmitters.Remove(R->Key);
	++MeshCounter;
}

// ---------------------------------------------------------------------------------------------------------------------
// Frame update

void AMCVoxelRenderer::UpdateRenderer(float DeltaSeconds, const FVector& CameraBlocks)
{
	if (!bMaterialsReady) InitMaterials();
	if (!World) return;
	const double T0 = FPlatformTime::Seconds();

	// 1. collect dirty chunks from the world
	for (const FMCChunkPos& C : World->TakeDirtyChunks())
	{
		if (!World->GetChunk(C)) DropChunk(C);
		else PendingChunks.Add(C);
	}

	// 2. apply finished meshes (bounded per frame)
	TSharedPtr<FMCMeshResult> R;
	int32 Applied = 0;
	while (Applied < 48 && Results->Dequeue(R)) { ApplyResult(R); ++Applied; }

	// 3. launch new meshing tasks, nearest first
	const int32 MaxInFlight = FMath::Clamp(FPlatformMisc::NumberOfWorkerThreadsToSpawn() * 2, 4, 32);
	if (PendingChunks.Num() > 0 && InFlight.Num() < MaxInFlight)
	{
		const FMCChunkPos Cam = FMCChunkPos::FromBlock(MC::FloorToInt(CameraBlocks.X), MC::FloorToInt(CameraBlocks.Y));
		TArray<FMCChunkPos> Sorted = PendingChunks.Array();
		Sorted.Sort([&](const FMCChunkPos& A, const FMCChunkPos& B)
		{
			return FMath::Square(A.X - Cam.X) + FMath::Square(A.Y - Cam.Y) < FMath::Square(B.X - Cam.X) + FMath::Square(B.Y - Cam.Y);
		});
		const int32 CamGroup = FMath::Clamp(MCRender::GroupOfZ(MC::FloorToInt(CameraBlocks.Z)), 0, MCRender::NumGroups - 1);
		for (const FMCChunkPos& C : Sorted)
		{
			if (InFlight.Num() >= MaxInFlight || FPlatformTime::Seconds() - T0 > 0.004) break;
			FMCChunk* Ch = World->GetChunk(C);
			if (!Ch) { PendingChunks.Remove(C); continue; }
			if (FMath::Abs(C.X - Cam.X) > RenderDistance + 1 || FMath::Abs(C.Y - Cam.Y) > RenderDistance + 1) { DropChunk(C); Ch->MeshDirty = 0; continue; }
			if (!CanMesh(C)) continue;
			bool bBlocked = false;
			// camera group first so edits under the player appear instantly
			for (int32 k = 0; k < MCRender::NumGroups; ++k)
			{
				const int32 G = k == 0 ? CamGroup : (k <= CamGroup ? k - 1 : k);
				const uint32 Mask = 0xFu << (G * 4);
				if (!(Ch->MeshDirty & Mask)) continue;
				if (InFlight.Contains(MakeKey(C, G))) { bBlocked = true; continue; }
				Ch->MeshDirty &= ~Mask;
				LaunchMesh(C, G);
			}
			if (!bBlocked && (Ch->MeshDirty & 0xFFFFFF) == 0) PendingChunks.Remove(C);
		}
	}

	// 4. periodic housekeeping: rebuild groups whose proxies lost their data, drop groups of unloaded chunks
	StatTimer += DeltaSeconds;
	if (StatTimer >= 1.0)
	{
		StatTimer = 0.0;
		StatMeshesThisSecond = MeshCounter;
		MeshCounter = 0;
		TArray<int64> Drop;
		for (auto& Pair : Groups)
		{
			const FMCChunkPos CP = KeyChunk(Pair.Key);
			FMCChunk* Ch = World->GetChunk(CP);
			if (!Ch) { Drop.Add(Pair.Key); continue; }
			if (Pair.Value && Pair.Value->bNeedsRebuild)
			{
				Pair.Value->bNeedsRebuild = false;
				Ch->MeshDirty |= 0xFu << ((Pair.Key & 7) * 4);
				PendingChunks.Add(CP);
			}
		}
		for (int64 K : Drop) ReleaseGroup(K);
	}
	StatComponents = Groups.Num();
	// outstanding work inside the render distance; the ring just beyond it waits for neighbours that are
	// generated but never lit (the world lights one ring less than it generates), so it is not counted
	{
		const FMCChunkPos Cam = FMCChunkPos::FromBlock(MC::FloorToInt(CameraBlocks.X), MC::FloorToInt(CameraBlocks.Y));
		int32 Visible = 0;
		for (const FMCChunkPos& C : PendingChunks)
			if (FMath::Max(FMath::Abs(C.X - Cam.X), FMath::Abs(C.Y - Cam.Y)) <= RenderDistance) ++Visible;
		StatPendingMeshes = Visible + InFlight.Num();
	}

	UpdateLights(CameraBlocks);
	StatMeshMs = (FPlatformTime::Seconds() - T0) * 1000.0;
}

void AMCVoxelRenderer::UpdateVisibility(const FVector& CameraBlocks)
{
	// Frustum and distance culling are handled by the engine through per-group bounds.
}

void AMCVoxelRenderer::UpdateLights(const FVector& CameraBlocks)
{
	LightTimer += 1.0 / 60.0;
	if (LightTimer < 0.2) return;
	LightTimer = 0.0;
	struct FCand { FIntVector4 E; double D; };
	TArray<FCand> Cands;
	const int32 CamCX = MC::FloorToInt(CameraBlocks.X) >> 4, CamCY = MC::FloorToInt(CameraBlocks.Y) >> 4;
	for (const auto& Pair : GroupEmitters)
	{
		const FMCChunkPos CP = KeyChunk(Pair.Key);
		if (FMath::Abs(CP.X - CamCX) > 2 || FMath::Abs(CP.Y - CamCY) > 2) continue;
		for (const FIntVector4& E : Pair.Value)
		{
			const double D = FVector::DistSquared(FVector(E.X + 0.5, E.Y + 0.5, E.Z + 0.5), CameraBlocks);
			if (D < LightRangeBlocks * LightRangeBlocks) Cands.Add({ E, D });
		}
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.D < B.D; });
	StatLights = FMath::Min(Cands.Num(), LightPool.Num());
	for (int32 i = 0; i < LightPool.Num(); ++i)
	{
		UPointLightComponent* PL = LightPool[i];
		if (i >= Cands.Num()) { if (PL->IsVisible()) PL->SetVisibility(false); continue; }
		const FIntVector4& E = Cands[i].E;
		const FMCBlockPos P(E.X, E.Y, E.Z);
		const FMCBlock& B = World->GetBlock(P);
		if (FMCBlocks::Info(World->GetState(P)).Light == 0) { PL->SetVisibility(false); continue; }
		PL->SetWorldLocation(P.Center());
		PL->SetLightColor(EmitterColor(B));
		PL->SetAttenuationRadius(E.W * 70.f);
		PL->SetIntensity(E.W * 3.f);
		PL->SetSourceRadius(8.f);
		if (!PL->IsVisible()) PL->SetVisibility(true);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Selection outline and crack overlay

void AMCVoxelRenderer::SetSelection(bool bShow, const FMCBlockPos& P, uint16 State, float BreakProgress)
{
	if (!Outline || !Crack || !World) return;
	if (!bShow || State == 0)
	{
		if (SelStage != -2) { Outline->ClearMesh(); Crack->ClearMesh(); SelStage = -2; SelState = 0; }
		return;
	}
	const int32 Stage = BreakProgress > 0.f ? FMath::Clamp((int32)(BreakProgress * 10.f), 0, 9) : -1;
	if (P == SelPos && State == SelState && Stage == SelStage) return;
	const bool bShapeChanged = !(P == SelPos) || State != SelState || SelStage == -2;
	SelPos = P; SelState = State;
	const FVector Loc = P.ToWorld();
	if (bShapeChanged)
	{
		TArray<FMCBox> Boxes;
		FMCBlocks::GetOutline(State, World, P, Boxes);
		TUniquePtr<FMCChunkMeshData> D = MakeUnique<FMCChunkMeshData>();
		FMCMeshLayerData& L = D->Layers[(int32)EMCLayer::Opaque];
		const float W = 0.0045f; // line half-width (blocks)
		auto AddBox = [&](FVector3f A, FVector3f B)
		{
			A -= FVector3f(W); B += FVector3f(W);
			const FVector3f C[8] = { {A.X,A.Y,A.Z},{B.X,A.Y,A.Z},{B.X,B.Y,A.Z},{A.X,B.Y,A.Z},{A.X,A.Y,B.Z},{B.X,A.Y,B.Z},{B.X,B.Y,B.Z},{A.X,B.Y,B.Z} };
			static const int32 F[6][4] = { {0,3,2,1},{4,5,6,7},{0,1,5,4},{2,3,7,6},{3,0,4,7},{1,2,6,5} };
			static const FVector3f Nrm[6] = { {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0} };
			for (int32 f = 0; f < 6; ++f)
			{
				const uint32 Base = (uint32)L.Vertices.Num();
				for (int32 k = 0; k < 4; ++k)
				{
					FMCMeshVertex V;
					V.Pos = C[F[f][k]] * 100.f;
					V.Normal = Nrm[f];
					V.Tangent = FMath::Abs(Nrm[f].Z) > 0.5f ? FVector3f(1, 0, 0) : FVector3f(0, 0, 1);
					V.UV0 = FVector2f(k == 1 || k == 2 ? 1.f : 0.f, k >= 2 ? 1.f : 0.f);
					V.UV1 = FVector2f(0.f, 0.f);
					V.UV2 = FVector2f(1.f, 0.f);
					V.Color = FColor(8, 8, 8, 255);
					L.Vertices.Add(V);
				}
				L.Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
			}
		};
		for (const FMCBox& Bx : Boxes)
		{
			const FVector3f Mn(Bx.Min), Mx(Bx.Max);
			// 12 edges
			AddBox(FVector3f(Mn.X, Mn.Y, Mn.Z), FVector3f(Mx.X, Mn.Y, Mn.Z)); AddBox(FVector3f(Mn.X, Mx.Y, Mn.Z), FVector3f(Mx.X, Mx.Y, Mn.Z));
			AddBox(FVector3f(Mn.X, Mn.Y, Mx.Z), FVector3f(Mx.X, Mn.Y, Mx.Z)); AddBox(FVector3f(Mn.X, Mx.Y, Mx.Z), FVector3f(Mx.X, Mx.Y, Mx.Z));
			AddBox(FVector3f(Mn.X, Mn.Y, Mn.Z), FVector3f(Mn.X, Mx.Y, Mn.Z)); AddBox(FVector3f(Mx.X, Mn.Y, Mn.Z), FVector3f(Mx.X, Mx.Y, Mn.Z));
			AddBox(FVector3f(Mn.X, Mn.Y, Mx.Z), FVector3f(Mn.X, Mx.Y, Mx.Z)); AddBox(FVector3f(Mx.X, Mn.Y, Mx.Z), FVector3f(Mx.X, Mx.Y, Mx.Z));
			AddBox(FVector3f(Mn.X, Mn.Y, Mn.Z), FVector3f(Mn.X, Mn.Y, Mx.Z)); AddBox(FVector3f(Mx.X, Mn.Y, Mn.Z), FVector3f(Mx.X, Mn.Y, Mx.Z));
			AddBox(FVector3f(Mn.X, Mx.Y, Mn.Z), FVector3f(Mn.X, Mx.Y, Mx.Z)); AddBox(FVector3f(Mx.X, Mx.Y, Mn.Z), FVector3f(Mx.X, Mx.Y, Mx.Z));
		}
		D->Bounds = FBox(FVector(-10), FVector(110));
		Outline->SetWorldLocation(Loc);
		Outline->SetMeshData(MoveTemp(D));
	}
	if (Stage != SelStage || bShapeChanged)
	{
		SelStage = Stage;
		if (Stage < 0) { Crack->ClearMesh(); return; }
		FMCBlockModel Model;
		FMCBlocks::BuildModel(State, World, P, Model);
		TArray<FMCBox> Boxes;
		for (const FMCModelBox& MB : Model.Boxes) Boxes.Add(MB.ToBox());
		if (Boxes.Num() == 0) FMCBlocks::GetOutline(State, World, P, Boxes);
		if (Boxes.Num() == 0) Boxes.Add(FMCBox(0, 0, 0, 1, 1, 1));
		TUniquePtr<FMCChunkMeshData> D = MakeUnique<FMCChunkMeshData>();
		FMCMeshLayerData& L = D->Layers[(int32)EMCLayer::Translucent];
		const float Layer = (float)MCTexSynth::CrackLayer(Stage);
		for (const FMCBox& Bx : Boxes)
		{
			const FMCBox B = Bx.Inflate(0.003);
			const FVector3f A(B.Min), Bm(B.Max);
			const FVector3f C[8] = { {A.X,A.Y,A.Z},{Bm.X,A.Y,A.Z},{Bm.X,Bm.Y,A.Z},{A.X,Bm.Y,A.Z},{A.X,A.Y,Bm.Z},{Bm.X,A.Y,Bm.Z},{Bm.X,Bm.Y,Bm.Z},{A.X,Bm.Y,Bm.Z} };
			static const int32 F[6][4] = { {0,3,2,1},{4,5,6,7},{0,1,5,4},{2,3,7,6},{3,0,4,7},{1,2,6,5} };
			static const FVector3f Nrm[6] = { {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0} };
			for (int32 f = 0; f < 6; ++f)
			{
				const uint32 Base = (uint32)L.Vertices.Num();
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector3f Pp = C[F[f][k]];
					FMCMeshVertex V;
					V.Pos = Pp * 100.f;
					V.Normal = Nrm[f];
					V.Tangent = FMath::Abs(Nrm[f].Z) > 0.5f ? FVector3f(1, 0, 0) : FVector3f(0, 0, 1);
					// planar projection keeps the crack pattern aligned with the block grid
					const FVector2f UV = FMath::Abs(Nrm[f].Z) > 0.5f ? FVector2f(Pp.X, 1.f - Pp.Y) : (FMath::Abs(Nrm[f].X) > 0.5f ? FVector2f(Pp.Y, 1.f - Pp.Z) : FVector2f(Pp.X, 1.f - Pp.Z));
					V.UV0 = UV;
					V.UV1 = FVector2f(Layer, 0.f);
					V.UV2 = FVector2f(1.f, 0.f);
					V.Color = FColor(255, 255, 255, 255);
					L.Vertices.Add(V);
				}
				L.Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
			}
		}
		D->Bounds = FBox(FVector(-10), FVector(110));
		Crack->SetWorldLocation(Loc);
		Crack->SetMeshData(MoveTemp(D));
	}
}

int32 AMCVoxelRenderer::CountDrawnSections(float Tolerance) const
{
	int32 N = 0;
	for (const auto& Pair : Groups)
		if (Pair.Value && Pair.Value->WasRecentlyRendered(Tolerance)) ++N;
	return N;
}
