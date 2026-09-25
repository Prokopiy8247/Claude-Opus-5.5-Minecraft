// Game manager actor: owns the three dimensions, runs the 20 Hz simulation, time/weather, spawning,
// dimension travel, the dragon fight, persistence and the environment (sun, sky, fog, clouds).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MCCore.h"
#include "Items/MCItems.h"
#include "World/MCWorld.h"
#include "MCGame.generated.h"

class FMCWorld;
class AMCEntity;
class AMCPlayer;
class AMCVoxelRenderer;
class AMCEnderDragon;
class UMCAudio;
class UMCParticles;
class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class AExponentialHeightFog;
class AVolumetricCloud;
class APostProcessVolume;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ACameraActor;
class UMCGameInstance;

USTRUCT()
struct FMCGameRules
{
	GENERATED_BODY()
	bool bDoDaylightCycle = true;
	bool bDoWeatherCycle = true;
	bool bDoMobSpawning = true;
	bool bKeepInventory = false;
	bool bMobGriefing = true;
	bool bDoFireTick = true;
	bool bNaturalRegeneration = true;
	bool bShowCoordinates = false;
	bool bDoImmediateRespawn = false;
	bool bFallDamage = true;
	bool bDrowningDamage = true;
	bool bFireDamage = true;
	bool bDoTileDrops = true;
	bool bDoMobLoot = true;
	int32 RandomTickSpeed = 3;
	int32 SpawnRadius = 10;
};

/** Persistent state of the End dragon fight. */
struct FMCDragonFight
{
	bool bDragonKilled = false;
	bool bPreviouslyKilled = false;
	bool bDragonSpawned = false;
	bool bEggPlaced = false;
	int32 GatewaysSpawned = 0;
	int32 RespawnStage = -1;
	int32 RespawnTimer = 0;
	TWeakObjectPtr<AMCEnderDragon> Dragon;
	int32 CrystalsAlive = 10;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCGame : public AActor
{
	GENERATED_BODY()
public:
	AMCGame();

	static AMCGame* Get(const UObject* WorldContext);

	// ---- configuration
	FString WorldName = TEXT("New World");
	FString SaveRoot;          // Saved/Opus55Worlds/<name>
	uint64 Seed = 0;
	EMCDifficulty Difficulty = EMCDifficulty::Normal;
	EMCGameMode DefaultGameMode = EMCGameMode::Creative;
	FMCGameRules Rules;
	int32 RenderDistance = 12;  // chunks
	int32 SimulationDistance = 8;
	bool bHardcore = false;
	bool bAllowCheats = true;

	// ---- state
	TUniquePtr<FMCWorld> Worlds[(int32)EMCDimension::Count];
	EMCDimension ActiveDim = EMCDimension::Overworld;
	UPROPERTY(Transient) TObjectPtr<AMCPlayer> Player = nullptr;
	UPROPERTY(Transient) TObjectPtr<AMCVoxelRenderer> Renderer = nullptr;
	UPROPERTY(Transient) TObjectPtr<UMCAudio> Audio = nullptr;
	UPROPERTY(Transient) TObjectPtr<UMCParticles> Particles = nullptr;
	int64 DayTime = 1000;        // 0..24000 ticks per day (6000 = noon)
	int64 GameTime = 0;
	bool bRaining = false, bThundering = false;
	int32 RainTime = 12000, ThunderTime = 24000, ClearWeatherTime = 0;
	float RainLevel = 0.f, PrevRainLevel = 0.f, ThunderLevel = 0.f, PrevThunderLevel = 0.f;
	FMCBlockPos WorldSpawn;
	FMCDragonFight DragonFight;
	bool bWorldReady = false;
	bool bPaused = false;
	bool bLoadingTerrain = true;
	double TickAccumulator = 0.0;
	float PartialTick = 0.f;     // interpolation alpha
	int64 NextEntityId = 1;
	int32 AutosaveTimer = 0;
	FMCRandom Rand;
	double LastTickMs = 0.0;
	double AvgTickMs = 0.0;
	/** Last ~120 server tick durations in ms (F3+F4 chart). */
	TArray<double> TickHistory;
	int32 FramesThisSecond = 0;
	int32 FPS = 0;
	double FPSTimer = 0.0;
	TArray<FString> ChatLog;
	TArray<double> ChatTimes;
	FString ActionBarText;
	double ActionBarTime = 0.0;
	FString TitleText, SubtitleText;
	double TitleTime = 0.0;
	bool bShowCredits = false;
	double CreditsTime = 0.0;
	bool bNightVisionFlash = false;
	FVector LastLightningPos = FVector::ZeroVector;
	float LightningFlash = 0.f;

	// ---- title screen / loading
	bool bTitleScreen = true;
	bool bLoading = false;
	FString LoadingText;
	float LoadingProgress = 0.f;
	float TitleYaw = 0.f;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> TitleCamera = nullptr;
	void StartTitleScreen();
	UMCGameInstance* GetMCInstance() const;
	/** Applies user options (render distance, FOV, volume...). */
	void ApplyOptions();
	/** Snapshot list of bosses for the boss bars. */
	void GetBossBars(TArray<class AMCMob*>& Out) const;
	int32 SleepTimer = 0;

	/** Start or load a world. Returns false if the save could not be read. */
	bool StartWorld(const FString& Name, uint64 InSeed, bool bLoad, EMCGameMode Mode);
	void SaveWorld(bool bAllChunks = true);
	void QuitToTitle();

	FMCWorld* GetMCWorld(EMCDimension D) const { return Worlds[(int32)D].Get(); }
	FMCWorld* ActiveWorld() const { return Worlds[(int32)ActiveDim].Get(); }
	FMCWorld* EnsureWorld(EMCDimension D);

	// ---- entities
	template<typename T> T* SpawnEntity(FMCWorld* W, const FVector& PosBlocks)
	{
		return Cast<T>(SpawnEntityOfClass(T::StaticClass(), W, PosBlocks));
	}
	AMCEntity* SpawnEntityOfClass(UClass* Cls, FMCWorld* W, const FVector& PosBlocks);
	AMCEntity* SpawnMob(FMCWorld* W, FName MobId, const FVector& PosBlocks, bool bNatural, int32 Variant = -1);
	void DestroyEntity(AMCEntity* E);

	// ---- dimension travel
	void ChangeDimension(AMCEntity* E, EMCDimension To, const FVector* ExactPos = nullptr);
	/** Nether portal: find or create a matching portal around the scaled position. */
	FVector FindOrCreatePortal(FMCWorld* Target, const FVector& FromPos, EMCDimension From);
	bool TryCreateNetherPortal(FMCWorld& W, const FMCBlockPos& FirePos);
	void OnEndPortalEntered(AMCEntity* E);

	// ---- time & weather
	bool IsDay() const { const int64 T = DayTime % 24000; return T < 12542 || T > 23460; }
	int32 GetSkyDarken() const;
	float GetSunAngle() const;      // 0..1 (Minecraft celestial angle)
	int32 GetMoonPhase() const { return (int32)((DayTime / 24000) % 8); }
	void SetTime(int64 T) { DayTime = T; }
	void SetWeather(int32 Kind, int32 DurationTicks); // 0 clear 1 rain 2 thunder
	bool IsRainingAt(const FMCBlockPos& P) const;
	float GetRainAt(const FVector& PosBlocks) const;

	// ---- messaging / UI hooks
	void AddChat(const FString& Msg);
	void ShowActionBar(const FString& Msg, double Seconds = 2.0);
	void ShowTitle(const FString& Title, const FString& Sub, double Seconds = 3.5);
	bool ExecuteCommand(const FString& Cmd, AMCPlayer* Source);
	TArray<FString> GetCommandSuggestions(const FString& Partial) const;

	// ---- effects
	void PlaySound(FMCWorld* W, FName Sound, const FVector& PosBlocks, float Volume = 1.f, float Pitch = 1.f);
	void SpawnParticles(FMCWorld* W, FName Type, const FVector& PosBlocks, int32 Count, float Spread, const FVector& Vel, FColor Color);
	void SpawnBlockParticles(FMCWorld* W, const FMCBlockPos& P, uint16 State, bool bBreak);
	void ScreenShake(float Amount) { Shake = FMath::Max(Shake, Amount); }
	float Shake = 0.f;
	void StrikeLightning(FMCWorld* W, const FVector& PosBlocks, bool bVisualOnly);

	// ---- dragon fight
	void TickDragonFight();
	void OnDragonKilled();
	void SpawnExitPortal(bool bActive);
	void SpawnNextGateway();
	int32 CountEndCrystals() const;

	// AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- environment
	UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Sun = nullptr;
	UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Moon = nullptr;
	UPROPERTY(Transient) TObjectPtr<ASkyLight> SkyLight = nullptr;
	UPROPERTY(Transient) TObjectPtr<ASkyAtmosphere> Atmosphere = nullptr;
	UPROPERTY(Transient) TObjectPtr<AExponentialHeightFog> Fog = nullptr;
	UPROPERTY(Transient) TObjectPtr<AVolumetricCloud> Clouds = nullptr;
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> PostProcess = nullptr;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SkyDome = nullptr;   // stars / end sky / nether haze
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RainMesh = nullptr;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SkyMID = nullptr;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RainMID = nullptr;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SunDisc = nullptr;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> MoonDisc = nullptr;
	void SetupEnvironment();
	void UpdateEnvironment(float DeltaSeconds);
	FLinearColor GetFogColor() const;

	// ---- mob spawning
	void TickSpawning(FMCWorld& W);
	bool CanSpawnAt(FMCWorld& W, FName Mob, const FMCBlockPos& P, bool bHostile) const;

private:
	void FixedTick();
	void TickTimeAndWeather();
	void TickEntities(FMCWorld& W);
	void UpdateStreaming();
	void LoadLevelData();
	void SaveLevelData();
	void SavePlayer();
	bool LoadPlayer();
	void PlacePlayerAtSpawn();
	void SetupNewPlayer();
	FVector FindEndSpawn();
	bool bPendingDimChange = false;
	double StreamTimer = 0.0;
	int32 SpawnTimer = 0;
};
