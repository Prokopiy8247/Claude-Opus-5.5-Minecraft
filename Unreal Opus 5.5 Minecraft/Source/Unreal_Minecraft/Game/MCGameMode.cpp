// Game mode, game instance (options + world list) and the player controller that maps raw input to Minecraft controls.
#include "Game/MCGameMode.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "UI/MCRootWidget.h"
#include "Render/MCVoxelRenderer.h"
#include "Audio/MCAudio.h"
#include "Engine/GameEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Components/AudioComponent.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCGameMode)

// ---------------------------------------------------------------------------------------------------------------------
// Game instance

void UMCGameInstance::Init()
{
	Super::Init();
	LoadOptions();
	if (FParse::Param(FCommandLine::Get(), TEXT("Opus55AutoStart")) || FParse::Param(FCommandLine::Get(), TEXT("Opus55Tour"))) bAutoStart = true;
	UE_LOG(LogOpus55, Log, TEXT("Game instance ready (options: render distance %d, FOV %.0f)"), Options.RenderDistance, Options.FOV);
}

void UMCGameInstance::Shutdown()
{
	SaveOptions();
	Super::Shutdown();
}

FString UMCGameInstance::SaveRoot() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Worlds"));
}

void UMCGameInstance::LoadOptions()
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Options.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;
	Root->TryGetNumberField(TEXT("MouseSensitivity"), Options.MouseSensitivity);
	Root->TryGetNumberField(TEXT("FOV"), Options.FOV);
	Root->TryGetNumberField(TEXT("GuiScale"), Options.GuiScale);
	Root->TryGetNumberField(TEXT("GraphicsQuality"), Options.GraphicsQuality);
	Root->TryGetNumberField(TEXT("RenderDistance"), Options.RenderDistance);
	Root->TryGetNumberField(TEXT("SimulationDistance"), Options.SimulationDistance);
	Root->TryGetNumberField(TEXT("MasterVolume"), Options.MasterVolume);
	Root->TryGetNumberField(TEXT("MusicVolume"), Options.MusicVolume);
	Root->TryGetNumberField(TEXT("Brightness"), Options.Brightness);
	Root->TryGetBoolField(TEXT("InvertMouse"), Options.bInvertMouse);
	Root->TryGetBoolField(TEXT("ViewBobbing"), Options.bViewBobbing);
	Root->TryGetBoolField(TEXT("ShowFPS"), Options.bShowFPS);
	Root->TryGetBoolField(TEXT("Clouds"), Options.bClouds);
	Root->TryGetBoolField(TEXT("Shadows"), Options.bShadows);
	Root->TryGetBoolField(TEXT("AutoJump"), Options.bAutoJump);
	Root->TryGetBoolField(TEXT("ToggleSprint"), Options.bToggleSprint);
	Root->TryGetBoolField(TEXT("ToggleSneak"), Options.bToggleSneak);
}

void UMCGameInstance::SaveOptions() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("MouseSensitivity"), Options.MouseSensitivity);
	Root->SetNumberField(TEXT("FOV"), Options.FOV);
	Root->SetNumberField(TEXT("GuiScale"), Options.GuiScale);
	Root->SetNumberField(TEXT("GraphicsQuality"), Options.GraphicsQuality);
	Root->SetNumberField(TEXT("RenderDistance"), Options.RenderDistance);
	Root->SetNumberField(TEXT("SimulationDistance"), Options.SimulationDistance);
	Root->SetNumberField(TEXT("MasterVolume"), Options.MasterVolume);
	Root->SetNumberField(TEXT("MusicVolume"), Options.MusicVolume);
	Root->SetNumberField(TEXT("Brightness"), Options.Brightness);
	Root->SetBoolField(TEXT("InvertMouse"), Options.bInvertMouse);
	Root->SetBoolField(TEXT("ViewBobbing"), Options.bViewBobbing);
	Root->SetBoolField(TEXT("ShowFPS"), Options.bShowFPS);
	Root->SetBoolField(TEXT("Clouds"), Options.bClouds);
	Root->SetBoolField(TEXT("Shadows"), Options.bShadows);
	Root->SetBoolField(TEXT("AutoJump"), Options.bAutoJump);
	Root->SetBoolField(TEXT("ToggleSprint"), Options.bToggleSprint);
	Root->SetBoolField(TEXT("ToggleSneak"), Options.bToggleSneak);
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	FFileHelper::SaveStringToFile(Out, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Opus55Options.json")));
}

TArray<FString> UMCGameInstance::ListWorlds() const
{
	TArray<FString> Out;
	const FString Root = SaveRoot();
	IFileManager::Get().FindFiles(Out, *(Root / TEXT("*")), false, true);
	TArray<FString> Valid;
	for (const FString& Dir : Out)
	{
		if (IFileManager::Get().FileExists(*(Root / Dir / TEXT("level.dat")))) Valid.Add(Dir);
	}
	Valid.Sort();
	return Valid;
}

// ---------------------------------------------------------------------------------------------------------------------
// Game mode

AMCGameMode::AMCGameMode()
{
	DefaultPawnClass = nullptr;             // AMCGame spawns and owns the player entity itself
	PlayerControllerClass = AMCPlayerController::StaticClass();
	bStartPlayersAsSpectators = false;
	bUseSeamlessTravel = false;
}

void AMCGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UE_LOG(LogOpus55, Log, TEXT("Opus 5.5 Minecraft game mode starting on %s"), *MapName);
}

void AMCGameMode::StartPlay()
{
	if (!Game)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Game = GetWorld()->SpawnActor<AMCGame>(AMCGame::StaticClass(), FTransform::Identity, P);
	}
	// AMCGame::BeginPlay (dispatched by Super::StartPlay) decides between the title screen and a world
	// requested on the command line or by the title screen's world list
	Super::StartPlay();
}

// ---------------------------------------------------------------------------------------------------------------------
// Player controller

AMCPlayerController::AMCPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AMCGame* AMCPlayerController::GetGame() const
{
	if (AMCGameMode* GM = Cast<AMCGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)) return GM->Game;
	return AMCGame::Get(this);
}

void AMCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// --- default bindings, matching Minecraft Java Edition
	Bindings[(int32)EMCAction::Forward] = EKeys::W;
	Bindings[(int32)EMCAction::Back] = EKeys::S;
	Bindings[(int32)EMCAction::Left] = EKeys::A;
	Bindings[(int32)EMCAction::Right] = EKeys::D;
	Bindings[(int32)EMCAction::Jump] = EKeys::SpaceBar;
	Bindings[(int32)EMCAction::Sneak] = EKeys::LeftShift;
	Bindings[(int32)EMCAction::Sprint] = EKeys::LeftControl;
	Bindings[(int32)EMCAction::Attack] = EKeys::LeftMouseButton;
	Bindings[(int32)EMCAction::Use] = EKeys::RightMouseButton;
	Bindings[(int32)EMCAction::PickBlock] = EKeys::MiddleMouseButton;
	Bindings[(int32)EMCAction::Drop] = EKeys::Q;
	Bindings[(int32)EMCAction::SwapHands] = EKeys::F;
	Bindings[(int32)EMCAction::Inventory] = EKeys::E;
	Bindings[(int32)EMCAction::Chat] = EKeys::T;
	Bindings[(int32)EMCAction::Command] = EKeys::Slash;
	Bindings[(int32)EMCAction::PlayerList] = EKeys::Tab;
	Bindings[(int32)EMCAction::Screenshot] = EKeys::F2;
	Bindings[(int32)EMCAction::Perspective] = EKeys::F5;
	Bindings[(int32)EMCAction::Fullscreen] = EKeys::F11;
	Bindings[(int32)EMCAction::Debug] = EKeys::F3;
	Bindings[(int32)EMCAction::Pause] = EKeys::Escape;
	const FKey Numbers[9] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 i = 0; i < 9; ++i) Bindings[(int32)EMCAction::Hotbar1 + i] = Numbers[i];
	UpdateInputMode();
}

void AMCPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Root.IsValid()) { if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef()); Root.Reset(); }
	if (UMCGameInstance* GI = Cast<UMCGameInstance>(GetGameInstance())) GI->SaveOptions();
	Super::EndPlay(Reason);
}

void AMCPlayerController::UpdateInputMode()
{
	const AMCGame* G = GetGame();
	AMCPlayer* P = G ? G->Player.Get() : nullptr;
	const bool bMenu = (P && P->IsMenuOpen()) || bChatOpen || bPaused || (G && G->bTitleScreen);
	bUIWantsMouse = bMenu;
	bShowMouseCursor = bMenu;
	if (bMenu)
	{
		// widgets get the mouse, unhandled keys (E, Esc, hotbar numbers) still reach InputKey
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Mode.SetHideCursorDuringCapture(false);
		if (Root.IsValid()) Mode.SetWidgetToFocus(Root->GetFocusTarget());
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}

bool AMCPlayerController::IsDown(EMCAction A) const
{
	const FKey& K = Bindings[(int32)A];
	if (!K.IsValid()) return false;
	return IsInputKeyDown(K);
}

bool AMCPlayerController::WasPressed(EMCAction A) const
{
	return PressedThisFrame.Contains(Bindings[(int32)A]);
}

bool AMCPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const FKey& K = Params.Key;
	const bool bPressed = Params.Event == IE_Pressed || Params.Event == IE_Repeat;
	AMCGame* G = GetGame();

	// typing into the chat / creative search: keys belong to the text box, only Escape reaches the game
	if (Root.IsValid() && (bChatOpen || Root->IsTextInputFocused()) && K != EKeys::Escape && !K.IsMouseButton()) return true;

	// always-available keys first (they work while paused / in menus too)
	if (Params.Event == IE_Pressed)
	{
		if (K == EKeys::Escape || K == Bindings[(int32)EMCAction::Pause])
		{
			AMCPlayer* P = G ? G->Player.Get() : nullptr;
			if (P && P->IsMenuOpen()) { P->CloseMenu(); UpdateInputMode(); return true; }
			if (bChatOpen) { bChatOpen = false; if (Root.IsValid()) Root->CloseChat(); UpdateInputMode(); return true; }
			if (G && G->bTitleScreen && Root.IsValid()) { /* title screen handles its own buttons */ }
			bPaused = !bPaused;
			if (G) G->bPaused = bPaused;
			UpdateInputMode();
			if (Root.IsValid()) Root->SetPaused(bPaused);
			return true;
		}
		if (K == EKeys::F3) { bF3Down = true; bF3Used = false; }
		if (bF3Down && K == EKeys::F4)
		{
			// game-mode switcher: Creative -> Survival -> Adventure -> Spectator -> Creative (same path as /gamemode)
			bF3Used = true;
			if (AMCPlayer* P = G ? G->Player.Get() : nullptr)
			{
				static const TCHAR* Modes[4] = { TEXT("survival"), TEXT("creative"), TEXT("adventure"), TEXT("spectator") };
				static const int32 Next[4] = { 2, 0, 3, 1 };   // indexed by EMCGameMode
				G->ExecuteCommand(FString::Printf(TEXT("gamemode %s"), Modes[Next[FMath::Clamp((int32)P->GameMode, 0, 3)]]), P);
			}
			return true;
		}
		if (bF3Down && K == EKeys::G) { bF3Used = true; bShowDebugCharts = !bShowDebugCharts; return true; }
		if (K == EKeys::F3) { if (!bF3Used) bShowDebug = !bShowDebug; return true; }
		if (K == EKeys::F1) { bHideHUD = !bHideHUD; return true; }
		if (K == Bindings[(int32)EMCAction::Perspective])
		{
			if (AMCPlayer* P = G ? G->Player.Get() : nullptr) { P->CameraMode = (P->CameraMode + 1) % 3; return true; }
		}
		if (K == Bindings[(int32)EMCAction::Chat] && !bChatOpen && G && !G->bTitleScreen)
		{
			bChatOpen = true;
			if (Root.IsValid()) Root->OpenChat(false);
			UpdateInputMode();
			return true;
		}
		if (K == Bindings[(int32)EMCAction::Command] && !bChatOpen && G && !G->bTitleScreen)
		{
			bChatOpen = true;
			if (Root.IsValid()) Root->OpenChat(true);
			UpdateInputMode();
			return true;
		}
		if (K == Bindings[(int32)EMCAction::Screenshot])
		{
			const FString Name = FString::Printf(TEXT("Opus55_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Name);
			FScreenshotRequest::RequestScreenshot(Path, false, false);
			if (G) G->AddChat(TEXT("Saved screenshot as ") + Name);
			return true;
		}
		if (K == Bindings[(int32)EMCAction::Inventory] && G && G->Player && !G->bTitleScreen)
		{
			if (bChatOpen || bPaused) return true;
			AMCPlayer* P = G->Player.Get();
			if (P->IsMenuOpen()) P->CloseMenu(); else P->OpenInventory();
			UpdateInputMode();
			return true;
		}
		// hotbar selection (over a hovered inventory slot: swap that slot with the hotbar slot)
		for (int32 i = 0; i < 9; ++i)
		{
			if (K == Bindings[(int32)EMCAction::Hotbar1 + i])
			{
				if (AMCPlayer* P = G ? G->Player.Get() : nullptr)
				{
					if (P->IsMenuOpen() && Root.IsValid() && Root->HotbarKeyOverSlot(i)) return true;
					if (!P->IsMenuOpen()) P->SelectSlot(i);
				}
				return true;
			}
		}
		// inventory hot keys over the hovered slot: Q throws (Ctrl+Q the stack), F swaps with the offhand
		if (AMCPlayer* P = G ? G->Player.Get() : nullptr)
		{
			if (P->IsMenuOpen() && Root.IsValid())
			{
				if (K == Bindings[(int32)EMCAction::Drop] && Root->ThrowOverSlot(IsInputKeyDown(EKeys::LeftControl))) return true;
				if (K == Bindings[(int32)EMCAction::SwapHands] && Root->HotbarKeyOverSlot(40)) return true;
			}
		}
	}
	if (Params.Event == IE_Released && K == EKeys::F3) bF3Down = false;
	if (Params.Event == IE_Pressed && !bUIWantsMouse)
	{
		if (K == EKeys::MouseScrollUp) { --ScrollDelta; return true; }
		if (K == EKeys::MouseScrollDown) { ++ScrollDelta; return true; }
	}

	// gameplay latches (consumed by PlayerTick)
	if (!bUIWantsMouse && G && G->Player)
	{
		if (K == Bindings[(int32)EMCAction::Jump] && Params.Event == IE_Pressed) bJumpPressedEdge = true;
		if (K == Bindings[(int32)EMCAction::Forward] && Params.Event == IE_Pressed) bForwardPressedEdge = true;
		if (K == Bindings[(int32)EMCAction::Attack])
		{
			if (Params.Event == IE_Pressed) bAttackPressedEdge = true;
		}
		if (K == Bindings[(int32)EMCAction::Use] && Params.Event == IE_Pressed) bUsePressedEdge = true;
		if (K == Bindings[(int32)EMCAction::PickBlock] && Params.Event == IE_Pressed) bPickEdge = true;
		if (K == Bindings[(int32)EMCAction::Drop] && Params.Event == IE_Pressed) bDropEdge = true;
		if (K == Bindings[(int32)EMCAction::SwapHands] && Params.Event == IE_Pressed) bSwapEdge = true;
	}
	if (Params.Event == IE_Pressed) PressedThisFrame.AddUnique(K);
	// UMG / Slate widgets need these too
	const bool bHandled = Super::InputKey(Params);
	return bHandled || K == EKeys::LeftMouseButton || K == EKeys::RightMouseButton;
}

void AMCPlayerController::HandleDebugCombo(const FKey& Key)
{
	if (Key == EKeys::F3) bF3Down = true;
}

void AMCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	AMCGame* G = GetGame();
	if (!G) { PressedThisFrame.Reset(); return; }

	// create the UI root once the viewport exists; the title screen runs before any player is spawned
	if (!Root.IsValid() && G)
	{
		Root = SNew(SMCRootWidget).Controller(this);
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
		UpdateInputMode();
	}
	// title screen <-> world: cursor and input routing follow the switch
	if (G->bTitleScreen != bWasTitleScreen)
	{
		bWasTitleScreen = G->bTitleScreen;
		UpdateInputMode();
	}

	if (Root.IsValid())
	{
		Root->SetDebugVisible(bShowDebug);
		Root->TickWidget(DeltaTime);
	}

	AMCPlayer* P = G->Player.Get();
	if (P)
	{
		// ---- look
		if (!bUIWantsMouse)
		{
			const UMCGameInstance* GI = Cast<UMCGameInstance>(GetGameInstance());
			const float Sens = GI ? FMath::Lerp(0.02f, 0.35f, GI->Options.MouseSensitivity) : 0.12f;
			float DX = 0.f, DY = 0.f;
			GetInputMouseDelta(DX, DY);
			const float Scale = Sens * (P->IsUsingSpyglass() ? 0.4f : 1.f) * (P->CameraMode == 0 ? 1.f : 0.8f);
			P->ViewYaw += DX * Scale * 10.f;
			// UE reports MouseY positive when the mouse moves away from the player: that looks up (positive pitch)
			P->ViewPitch = FMath::Clamp(P->ViewPitch + DY * Scale * 10.f * ((GI && GI->Options.bInvertMouse) ? -1.f : 1.f), -90.f, 90.f);
			P->Yaw = P->ViewYaw;
			P->Pitch = P->ViewPitch;

			// ---- movement input
			P->Input.Forward = (IsDown(EMCAction::Forward) ? 1.f : 0.f) - (IsDown(EMCAction::Back) ? 1.f : 0.f);
			P->Input.Strafe = (IsDown(EMCAction::Right) ? 1.f : 0.f) - (IsDown(EMCAction::Left) ? 1.f : 0.f);
			P->Input.bJump = IsDown(EMCAction::Jump);
			P->Input.bSneak = IsDown(EMCAction::Sneak);
			const bool bSprintKey = IsDown(EMCAction::Sprint);
			P->Input.bSprint = bSprintKey;
			P->Input.bJumpPressed = bJumpPressedEdge;
			P->Input.bForwardPressed = bForwardPressedEdge;
			P->Input.bAttackHeld = IsDown(EMCAction::Attack);
			P->Input.bAttackPressed = bAttackPressedEdge;
			P->Input.bUseHeld = IsDown(EMCAction::Use);
			P->Input.bUsePressed = bUsePressedEdge;
			P->Input.bPickPressed = bPickEdge;
			P->Input.bDropPressed = bDropEdge;
			P->Input.bDropStack = IsDown(EMCAction::Sprint);
			P->Input.bSwapPressed = bSwapEdge;
			P->ScrollHotbar(ScrollDelta);
		}
		else
		{
			P->Input = FMCPlayerInput();
			// menus still let the player click: the Slate widgets handle that themselves
		}
	}
	ScrollDelta = 0;
	bJumpPressedEdge = bForwardPressedEdge = bAttackPressedEdge = bUsePressedEdge = false;
	bPickEdge = bDropEdge = bSwapEdge = false;
	PressedThisFrame.Reset();
}
