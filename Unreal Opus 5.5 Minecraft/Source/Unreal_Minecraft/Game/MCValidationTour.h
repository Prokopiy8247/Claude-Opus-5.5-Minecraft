// Scripted self-test (-Opus55Tour): once the world has streamed in, drives the running game through a fixed list
// of scenes (overworld terrain, mob line-ups, first/third person, creative and survival screens, containers, night
// lighting, a village, the Sulfur Caves, the Nether and the End), saves a screenshot of each to Saved/Opus55Tour,
// writes tour_report.txt with per-scene render/tick statistics and quits.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MCValidationTour.generated.h"

class AMCGame;
class AMCPlayer;
class AMCPlayerController;
class AMCEntity;

UCLASS()
class UNREAL_MINECRAFT_API AMCValidationTour : public AActor
{
	GENERATED_BODY()
public:
	AMCValidationTour();
	/** -Opus55Tour on the command line. */
	static bool IsRequested();
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FStep
	{
		FString Name;                  // screenshot / report label
		float Settle = 1.5f;           // seconds between Enter and the screenshot
		bool bWaitStreaming = false;   // also wait for the chunk mesher and shader compiler to drain
		float MaxWait = 30.f;          // upper bound for the streaming wait
		bool bShowUI = true;           // capture the Slate UI with the frame
		TFunction<bool(AMCValidationTour&)> Enter;   // returns false when the scene cannot be staged (logged as SKIP)
		TFunction<void(AMCValidationTour&)> Leave;
	};
	TArray<FStep> Steps;
	int32 Current = -1;
	double StepTime = 0.0;
	double ShotTime = -1.0;
	double TotalTime = 0.0;
	double WarmupTime = 0.0;
	bool bSkipped = false;
	bool bFinished = false;
	int32 Shots = 0, Skips = 0;
	double FrameSum = 0.0, WorstFrame = 0.0;
	int32 Frames = 0;
	TArray<FString> Report;
	TArray<TWeakObjectPtr<AMCEntity>> Spawned;
	FString OutDir;
	FVector Stand = FVector::ZeroVector;   // block position the scenes are staged around
	int32 Floor = 64;                      // stage floor height (block Z)
	bool bLoadCheck = false;               // -Opus55LoadCheck: only report the reloaded world
	FIntVector TntPos = FIntVector::ZeroValue;
	FIntVector PortalBase = FIntVector::ZeroValue;   // bottom-left obsidian of the test portal
	FIntVector EndPortalCenter = FIntVector::ZeroValue;   // centre of the player-built end portal (frame height)
	FString HeldBeforePortal;
	int32 CountBlocks(const FIntVector& Min, const FIntVector& Max, bool bAir, uint16 BlockId = 0) const;

	void BuildSteps();
	AMCGame* Game() const;
	AMCPlayer* Player() const;
	AMCPlayerController* Controller() const;
	bool IsStreaming() const;
	void Cmd(const FString& Line);
	void Place(const FVector& PosBlocks, float Yaw, float Pitch);
	void LookAt(const FVector& TargetBlocks);
	void BuildStage();
	int32 SpawnLineup(const TArray<FName>& Mobs, double Distance, double Spacing);
	void ClearSpawned();
	bool FindCavePocket(FName Biome, FVector& Out) const;
	void CloseScreens();
	void Log(const FString& Line);
	void LogCensus();
	FString Stats() const;
	void Finish();
};
