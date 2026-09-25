// Game mode, game instance (world selection / settings persistence) and the player controller that turns
// raw keyboard & mouse input into Minecraft controls and drives the Slate UI.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Core/MCCore.h"
#include "MCGameMode.generated.h"

class AMCGame;
class SMCRootWidget;

/** Persistent user options (Saved/Opus55Options.ini style, serialised as JSON). */
USTRUCT()
struct FMCOptions
{
	GENERATED_BODY()
	UPROPERTY() float MouseSensitivity = 0.5f;
	UPROPERTY() float FOV = 70.f;
	UPROPERTY() int32 RenderDistance = 12;
	UPROPERTY() int32 SimulationDistance = 8;
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float MusicVolume = 0.5f;
	UPROPERTY() bool bInvertMouse = false;
	UPROPERTY() bool bViewBobbing = true;
	UPROPERTY() bool bShowFPS = false;
	UPROPERTY() int32 GuiScale = 0;          // 0 = auto
	UPROPERTY() float Brightness = 0.5f;
	UPROPERTY() int32 GraphicsQuality = 2;   // 0 fast, 1 fancy, 2 fabulous (realistic)
	UPROPERTY() bool bClouds = true;
	UPROPERTY() bool bShadows = true;
	UPROPERTY() bool bAutoJump = false;
	UPROPERTY() bool bToggleSprint = false;
	UPROPERTY() bool bToggleSneak = false;
};

UCLASS()
class UNREAL_MINECRAFT_API UMCGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
	FMCOptions Options;
	void LoadOptions();
	void SaveOptions() const;
	FString SaveRoot() const;
	/** Existing worlds (folder names) sorted by last played. */
	TArray<FString> ListWorlds() const;
	/** Requested by the title screen; consumed by AMCGame. */
	FString PendingWorld;
	uint64 PendingSeed = 0;
	bool bPendingLoad = false;
	int32 PendingMode = 1;
	bool bAutoStart = false;      // -Opus55AutoStart / validation runs
};

UCLASS()
class UNREAL_MINECRAFT_API AMCGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AMCGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	UPROPERTY(Transient) TObjectPtr<AMCGame> Game = nullptr;
};

/** Key actions with default bindings matching Minecraft Java Edition. */
enum class EMCAction : uint8
{
	Forward, Back, Left, Right, Jump, Sneak, Sprint, Attack, Use, PickBlock, Drop, SwapHands, Inventory,
	Chat, Command, PlayerList, Screenshot, Perspective, Fullscreen, Debug, Pause, Hotbar1, Count = Hotbar1 + 9
};

UCLASS()
class UNREAL_MINECRAFT_API AMCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AMCPlayerController();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	AMCGame* GetGame() const;
	/** UI captures the mouse (screens open) or the game does (mouse look). */
	void UpdateInputMode();
	void ToggleDebug() { bShowDebug = !bShowDebug; }

	bool bShowDebug = false;
	bool bShowDebugCharts = false;
	bool bHideHUD = false;
	bool bChatOpen = false;
	bool bPaused = false;
	bool bUIWantsMouse = false;
	bool bWasTitleScreen = false;
	int32 SprintToggleState = 0;
	FKey Bindings[(int32)EMCAction::Count];
	TSharedPtr<SMCRootWidget> Root;

	// raw input latched between frames
	bool bJumpPressedEdge = false, bForwardPressedEdge = false, bAttackPressedEdge = false, bUsePressedEdge = false;
	bool bPickEdge = false, bDropEdge = false, bSwapEdge = false;
	int32 ScrollDelta = 0;
	TArray<FKey> PressedThisFrame;
	bool bF3Down = false, bF3Used = false;

private:
	void HandleGameplayKeys(float DeltaTime);
	void HandleDebugCombo(const FKey& Key);
	bool IsDown(EMCAction A) const;
	bool WasPressed(EMCAction A) const;
	double LastSpaceTime = -10.0, LastForwardTime = -10.0;
};
