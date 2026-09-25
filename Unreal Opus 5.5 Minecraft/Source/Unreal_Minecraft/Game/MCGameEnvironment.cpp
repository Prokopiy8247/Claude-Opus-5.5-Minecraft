// Environment: physically based sun / moon / sky atmosphere / volumetric clouds / height fog / post process driven
// by the Minecraft day cycle, weather, dimension, biome, underwater & status effects.
#include "Game/MCGame.h"
#include "Game/MCGameMode.h"
#include "Game/MCPlayer.h"
#include "World/MCWorld.h"
#include "Gen/MCBiomes.h"
#include "Render/MCVoxelRenderer.h"
#include "Render/MCParticles.h"
#include "Render/MCAssets.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"

namespace
{
	template<typename T>
	T* SpawnEnv(UWorld* W, const TCHAR* Name)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.Name = MakeUniqueObjectName(W, T::StaticClass(), FName(Name));
		return W->SpawnActor<T>(T::StaticClass(), FTransform::Identity, SP);
	}

	FLinearColor Lerp3(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * FMath::Clamp(T, 0.f, 1.f); }
}

void AMCGame::SetupEnvironment()
{
	UWorld* W = GetWorld();
	if (!W) return;
	// ---- sun
	Sun = SpawnEnv<ADirectionalLight>(W, TEXT("Opus55Sun"));
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(9.f);
		L->SetLightColor(FLinearColor(1.f, 0.96f, 0.9f));
		L->SetAtmosphereSunLight(true);
		L->SetAtmosphereSunLightIndex(0);
		L->SetCastShadows(true);
		L->SetDynamicShadowDistanceMovableLight(20000.f);
		L->SetDynamicShadowCascades(4);
		L->bCastCloudShadows = true;
		L->SetLightSourceAngle(0.6f);
		L->MarkRenderStateDirty();
	}
	// ---- moon (second atmosphere light)
	Moon = SpawnEnv<ADirectionalLight>(W, TEXT("Opus55Moon"));
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(0.25f);
		L->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
		L->SetAtmosphereSunLight(true);
		L->SetAtmosphereSunLightIndex(1);
		L->SetCastShadows(true);
		L->SetDynamicShadowDistanceMovableLight(8000.f);
	}
	// ---- sky atmosphere
	Atmosphere = SpawnEnv<ASkyAtmosphere>(W, TEXT("Opus55Atmosphere"));
	if (USkyAtmosphereComponent* A = Atmosphere->GetComponent())
	{
		A->SetMobility(EComponentMobility::Movable);
		A->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtAbsoluteWorldOrigin;
		A->SetRayleighScatteringScale(0.0331f);
		A->SetMieScatteringScale(0.004f);
	}
	// ---- sky light (real time capture of the atmosphere)
	SkyLight = SpawnEnv<ASkyLight>(W, TEXT("Opus55SkyLight"));
	if (USkyLightComponent* S = SkyLight->GetLightComponent())
	{
		S->SetMobility(EComponentMobility::Movable);
		S->bRealTimeCapture = true;
		S->SetIntensity(1.0f);
		S->SetLowerHemisphereColor(FLinearColor(0.02f, 0.02f, 0.025f));
		S->bLowerHemisphereIsBlack = false;
		S->SetCastShadows(true);
		S->RecaptureSky();
	}
	// ---- height fog
	Fog = SpawnEnv<AExponentialHeightFog>(W, TEXT("Opus55Fog"));
	if (UExponentialHeightFogComponent* F = Fog->GetComponent())
	{
		F->SetMobility(EComponentMobility::Movable);
		F->SetFogDensity(0.004f);
		F->SetFogHeightFalloff(0.08f);
		F->SetStartDistance(2000.f);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.4f);
		F->SetVolumetricFogExtinctionScale(0.6f);
		F->SetWorldLocation(FVector(0, 0, 6400.f));
	}
	// ---- volumetric clouds (engine default cloud material when available)
	Clouds = SpawnEnv<AVolumetricCloud>(W, TEXT("Opus55Clouds"));
	if (UVolumetricCloudComponent* C = Clouds->FindComponentByClass<UVolumetricCloudComponent>())
	{
		UMaterialInterface* CM = MCAssets::Material(TEXT("/Game/Opus55Minecraft/Materials/M_MCClouds.M_MCClouds"));
		if (!CM) CM = MCAssets::Material(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
		if (!CM) CM = MCAssets::Material(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud.m_SimpleVolumetricCloud"));
		if (CM) C->SetMaterial(CM);
		C->SetLayerBottomAltitude(1.9f);  // km above the planet top (~190 blocks)
		C->SetLayerHeight(3.5f);
	}
	// ---- post process
	PostProcess = SpawnEnv<APostProcessVolume>(W, TEXT("Opus55PostProcess"));
	PostProcess->bUnbound = true;
	PostProcess->Priority = 10.f;
	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_AutoExposureMethod = true; PP.AutoExposureMethod = AEM_Histogram;
	PP.bOverride_AutoExposureMinBrightness = true; PP.AutoExposureMinBrightness = 0.35f;
	PP.bOverride_AutoExposureMaxBrightness = true; PP.AutoExposureMaxBrightness = 2.2f;
	PP.bOverride_AutoExposureSpeedUp = true; PP.AutoExposureSpeedUp = 2.5f;
	PP.bOverride_AutoExposureSpeedDown = true; PP.AutoExposureSpeedDown = 1.2f;
	PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = -0.4f;
	PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.55f;
	PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.3f;
	PP.bOverride_MotionBlurAmount = true; PP.MotionBlurAmount = 0.f;
	PP.bOverride_LensFlareIntensity = true; PP.LensFlareIntensity = 0.15f;
	PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = 0.f;
	PP.bOverride_ColorSaturation = true; PP.ColorSaturation = FVector4(1.08, 1.08, 1.08, 1.0);
	PP.bOverride_ColorContrast = true; PP.ColorContrast = FVector4(1.04, 1.04, 1.04, 1.0);
	PP.bOverride_AmbientOcclusionIntensity = true; PP.AmbientOcclusionIntensity = 0.55f;
	PP.bOverride_AmbientOcclusionRadius = true; PP.AmbientOcclusionRadius = 120.f;
	PP.bOverride_FilmGrainIntensity = true; PP.FilmGrainIntensity = 0.f;

	// ---- stars / end sky dome
	SkyDome = NewObject<UStaticMeshComponent>(this, TEXT("SkyDome"));
	SkyDome->SetupAttachment(RootComponent);
	if (UStaticMesh* Sphere = MCAssets::Mesh(TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"))) SkyDome->SetStaticMesh(Sphere);
	else SkyDome->SetStaticMesh(MCAssets::Sphere());
	SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDome->SetCastShadow(false);
	SkyDome->bVisibleInRayTracing = false;
	SkyDome->SetWorldScale3D(FVector(SkyDome->GetStaticMesh() && SkyDome->GetStaticMesh()->GetName().Contains(TEXT("SkySphere")) ? 400.f : 4000.f));
	if (UMaterialInterface* SM = MCAssets::Material(TEXT("/Game/Opus55Minecraft/Materials/M_MCSky.M_MCSky")))
	{
		SkyMID = UMaterialInstanceDynamic::Create(SM, this);
		SkyDome->SetMaterial(0, SkyMID);
		SkyDome->RegisterComponent();
	}
	// sun & moon discs are drawn by the sky atmosphere / sky material
}

FLinearColor AMCGame::GetFogColor() const
{
	const FMCWorld* W = ActiveWorld();
	if (!W || !Player) return FLinearColor(0.6f, 0.75f, 1.f);
	const FMCBiomeDef& B = FMCBiomes::Get(W->GetBiome(Player->BlockPos()));
	if (W->Dim == EMCDimension::Nether) return FLinearColor(B.Fog) * 0.6f;
	if (W->Dim == EMCDimension::End) return FLinearColor(0.06f, 0.04f, 0.08f);
	const float Day = 1.f - GetSkyDarken() / 11.f;
	FLinearColor C = FLinearColor(B.Fog);
	C = Lerp3(FLinearColor(0.02f, 0.025f, 0.05f), C, Day);
	C = Lerp3(C, FLinearColor(0.35f, 0.38f, 0.42f) * Day, RainLevel * 0.6f);
	return C;
}

void AMCGame::UpdateEnvironment(float DeltaSeconds)
{
	const FMCWorld* W = ActiveWorld();
	const EMCDimension Dim = W ? W->Dim : EMCDimension::Overworld;
	const bool bOver = Dim == EMCDimension::Overworld;
	const float SunAngle = GetSunAngle();
	const float A = SunAngle * 2.f * PI;
	// sun direction: rises in the east (+X), sets in the west, slight southern tilt
	const FVector SunDir = FVector(-FMath::Sin(A), 0.25f, FMath::Cos(A)).GetSafeNormal();
	const float Elev = SunDir.Z;
	const float DayF = FMath::Clamp(Elev * 5.f + 0.35f, 0.f, 1.f);
	const float Rain = FMath::Lerp(PrevRainLevel, RainLevel, PartialTick);
	const float Thunder = FMath::Lerp(PrevThunderLevel, ThunderLevel, PartialTick);

	// status effects of the player
	float NightVision = 0.f, Darkness = 0.f, Blind = 0.f;
	bool bUnderWater = false, bInLava = false, bPowderSnow = false;
	if (Player && !bTitleScreen)
	{
		if (const FMCEffectInstance* NV = Player->GetEffect(EMCEffect::NightVision)) NightVision = NV->Duration > 200 ? 1.f : 0.7f + FMath::Sin((NV->Duration - PartialTick) * PI * 0.2f) * 0.3f;
		if (const FMCEffectInstance* D = Player->GetEffect(EMCEffect::Darkness)) Darkness = FMath::Max(0.f, FMath::Cos((D->Duration % 40) / 40.f * 2.f * PI) * 0.45f + 0.45f);
		if (Player->HasEffect(EMCEffect::Blindness)) Blind = 1.f;
		const FVector Eye = Player->Camera ? Player->Camera->GetComponentLocation() * MC::InvBlockSize : Player->GetEyePos();
		const FMCBlockPos EP(MC::FloorToInt(Eye.X), MC::FloorToInt(Eye.Y), MC::FloorToInt(Eye.Z));
		if (W)
		{
			const FMCStateInfo& I = FMCBlocks::Info(W->GetState(EP));
			bUnderWater = (I.Block == FMCBlocks::C.WaterId || (I.Flags & MCB_Waterlogged)) && Eye.Z < EP.Z + W->GetFluidHeight(EP);
			bInLava = I.Block == FMCBlocks::C.LavaId;
			bPowderSnow = FMCBlocks::Get(I.Block).Name == TEXT("powder_snow");
		}
	}

	// ---- lights
	if (Sun)
	{
		Sun->SetActorRotation((-SunDir).Rotation());
		if (ULightComponent* L = Sun->GetLightComponent())
		{
			const float Sunset = FMath::Clamp(1.f - FMath::Abs(Elev) * 4.f, 0.f, 1.f);
			L->SetIntensity(bOver ? 10.f * DayF * (1.f - Rain * 0.75f) * (1.f - Thunder * 0.5f) : 0.f);
			L->SetLightColor(Lerp3(FLinearColor(1.f, 0.97f, 0.92f), FLinearColor(1.f, 0.62f, 0.35f), Sunset));
			L->SetVisibility(bOver);
		}
	}
	if (Moon)
	{
		Moon->SetActorRotation(SunDir.Rotation());
		if (ULightComponent* L = Moon->GetLightComponent())
		{
			const float MoonF = FMath::Clamp(-Elev * 5.f + 0.2f, 0.f, 1.f);
			const int32 Phase = GetMoonPhase();
			const float PhaseF = 1.f - FMath::Abs(Phase - 4) / 4.f * 0.8f; // full moon brightest
			L->SetIntensity(bOver ? 0.6f * MoonF * PhaseF * (1.f - Rain * 0.7f) : 0.f);
			L->SetLightColor(FLinearColor(0.62f, 0.72f, 1.f));
			L->SetVisibility(bOver);
		}
	}
	if (SkyLight)
	{
		if (USkyLightComponent* S = SkyLight->GetLightComponent())
		{
			float I = 1.f;
			FLinearColor Tint = FLinearColor::White;
			if (Dim == EMCDimension::Nether) { I = 0.6f; Tint = FLinearColor(1.f, 0.55f, 0.35f); }
			else if (Dim == EMCDimension::End) { I = 0.45f; Tint = FLinearColor(0.6f, 0.45f, 0.9f); }
			// daylight fill strong enough that faces away from the sun keep their colour (Minecraft's sides stay readable)
			else I = FMath::Lerp(0.14f, 1.6f, DayF) * (1.f - Rain * 0.35f);
			I = FMath::Lerp(I, 1.2f, NightVision * 0.6f);
			S->SetIntensity(I);
			S->SetLightColor(Tint);
			if (!bOver) S->SetLowerHemisphereColor(Tint * 0.2f);
		}
	}
	if (Atmosphere) Atmosphere->GetComponent()->SetVisibility(bOver);
	if (Clouds) Clouds->SetActorHiddenInGame(!bOver);

	// ---- fog
	if (Fog)
	{
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			FLinearColor Col = GetFogColor();
			float Density = 0.0025f + Rain * 0.01f;
			float Start = 1600.f;
			float Falloff = 0.07f;
			const float RD = (float)RenderDistance * 16.f * MC::BlockSizeF;
			if (Dim == EMCDimension::Nether) { Density = 0.0055f; Start = 1200.f; Falloff = 0.001f; }
			else if (Dim == EMCDimension::End) { Density = 0.01f; Start = 1500.f; Falloff = 0.001f; }
			// the edge of the render distance fades into the fog
			Start = FMath::Min(Start, RD * 0.6f);
			if (bUnderWater)
			{
				const FMCBiomeDef& B = FMCBiomes::Get(W->GetBiome(Player->BlockPos()));
				Col = FLinearColor(B.WaterFog) * 2.f + FLinearColor(0.02f, 0.05f, 0.12f);
				Col *= FMath::Lerp(0.25f, 1.f, DayF);
				Density = 0.12f; Start = 0.f; Falloff = 0.0001f;
			}
			if (bInLava) { Col = FLinearColor(0.6f, 0.1f, 0.0f); Density = 3.f; Start = 0.f; Falloff = 0.0001f; }
			if (bPowderSnow) { Col = FLinearColor(0.62f, 0.73f, 0.8f); Density = 2.f; Start = 0.f; Falloff = 0.0001f; }
			if (Blind > 0.f || Darkness > 0.f) { Col = FLinearColor::Black; Density = FMath::Max(Density, 0.25f * FMath::Max(Blind, Darkness)); Start = 0.f; Falloff = 0.0001f; }
			F->SetFogInscatteringColor(Col);
			F->SetDirectionalInscatteringColor(bOver ? FLinearColor(1.f, 0.9f, 0.7f) * DayF * 0.4f : FLinearColor::Black);
			F->SetFogDensity(Density);
			F->SetStartDistance(Start);
			F->SetFogHeightFalloff(Falloff);
			F->SetFogMaxOpacity(1.f);
			F->SetVolumetricFog(bOver && !bUnderWater);
			if (Player && !bTitleScreen) F->SetWorldLocation(FVector(0, 0, Player->Pos.Z * MC::BlockSize - 400.f));
		}
	}

	// ---- post process
	if (PostProcess)
	{
		FPostProcessSettings& PP = PostProcess->Settings;
		const bool bNight = bOver && DayF < 0.2f;
		// EV100 floor: at night the eye may not adapt all the way up, so darkness stays dark away from lights
		PP.AutoExposureMinBrightness = Blind > 0.f ? 0.05f : (NightVision > 0.f ? 0.f : (bNight ? 0.55f : 0.35f));
		// the histogram pulls a grass-dominated frame up to mid grey; a negative bias keeps sunlit faces near their
		// texture colour (Minecraft shows full-light faces at albedo) instead of washing them out
		PP.AutoExposureMaxBrightness = 2.2f;
		PP.AutoExposureBias = -0.4f + NightVision * 1.5f - Darkness * 2.f + (LightningFlash > 0.f ? 0.5f * LightningFlash : 0.f);
		PP.bOverride_SceneColorTint = true;
		FLinearColor Tint = FLinearColor::White;
		if (bUnderWater) Tint = FLinearColor(0.7f, 0.85f, 1.f);
		if (bInLava) Tint = FLinearColor(1.f, 0.4f, 0.1f);
		if (Player && !bTitleScreen)
		{
			// portal overlay, freezing and hurt vignette
			const float Portal = FMath::Lerp(Player->PrevPortalOverlay, Player->PortalOverlay, PartialTick);
			if (Portal > 0.f) Tint = Lerp3(Tint, FLinearColor(0.8f, 0.4f, 1.f), Portal * 0.6f);
			if (Player->FreezeTicks > 0) Tint = Lerp3(Tint, FLinearColor(0.75f, 0.9f, 1.f), FMath::Min(1.f, Player->FreezeTicks / 140.f) * 0.5f);
			PP.VignetteIntensity = 0.3f + (Player->HurtTime > 0 ? Player->HurtTime / 10.f * 0.3f : 0.f) + (Player->IsUsingSpyglass() ? 1.5f : 0.f);
		}
		PP.SceneColorTint = Tint;
	}

	// ---- rain / snow particles around the camera
	if (W && Particles && bOver && Rain > 0.05f)
	{
		const FVector Cam = (bTitleScreen && TitleCamera) ? TitleCamera->GetActorLocation() * MC::InvBlockSize : (Player ? Player->GetEyePos() : FVector::ZeroVector);
		const int32 Drops = FMath::RoundToInt(Rain * 90.f * FMath::Min(1.f, DeltaSeconds * 60.f));
		for (int32 i = 0; i < Drops; ++i)
		{
			const FVector P = Cam + FVector(FMath::FRandRange(-14.f, 14.f), FMath::FRandRange(-14.f, 14.f), FMath::FRandRange(4.f, 14.f));
			const FMCBlockPos BP(MC::FloorToInt(P.X), MC::FloorToInt(P.Y), MC::FloorToInt(P.Z));
			if (!W->IsReadyAt(BP)) continue;
			const int32 Top = W->GetHeight(BP.X, BP.Y);
			if (P.Z < Top + 1) continue;
			const FMCBiomeDef& B = FMCBiomes::Get(W->GetBiome(FMCBlockPos(BP.X, BP.Y, Top)));
			if (B.bDry) continue;
			const bool bSnow = B.bSnowy || B.Temperature < 0.15f;
			Particles->Spawn(bSnow ? TEXT("snow_fall") : TEXT("rain_fall"), P, 1, 0.f, FVector::ZeroVector, FColor::White);
			if (!bSnow && FMath::FRand() < 0.15f) Particles->Spawn(TEXT("rain_splash"), FVector(P.X, P.Y, Top + 1.02), 1, 0.f, FVector::ZeroVector, FColor::White);
		}
		if (Audio && FMath::FRand() < DeltaSeconds * 2.f && Player) {}
	}

	// ---- voxel material globals
	if (Renderer)
	{
		// the Brightness option ("Moody".."Bright") raises the ambient floor like Minecraft's gamma slider
		const UMCGameInstance* GIB = GetMCInstance();
		float Ambient = 0.03f + (GIB ? GIB->Options.Brightness : 0.5f) * 0.1f;
		if (Dim == EMCDimension::Nether) Ambient = 0.35f;
		else if (Dim == EMCDimension::End) Ambient = 0.3f;
		Ambient = FMath::Max(Ambient, NightVision * 0.6f);
		Renderer->SetGlobalParams(1.f - GetSkyDarken() / 15.f, Ambient, NightVision, (float)((GameTime + PartialTick) / 20.0), Rain, GetFogColor());
	}
	if (SkyMID)
	{
		SkyMID->SetScalarParameterValue(TEXT("Night"), bOver ? 1.f - DayF : 0.f);
		SkyMID->SetScalarParameterValue(TEXT("Dimension"), (float)Dim);
		SkyMID->SetScalarParameterValue(TEXT("Rain"), Rain);
		SkyMID->SetScalarParameterValue(TEXT("Time"), (float)(GameTime / 20.0));
		if (Player && !bTitleScreen) SkyDome->SetWorldLocation(Player->GetActorLocation());
	}
}
