// Slate UI root: HUD (hotbar, health, hunger, XP, boss bars, crosshair), chat, debug overlay, in-game screens
// (inventory + creative tabs with search, container screens, furnace/brewing/etc.), title / world select /
// create world / pause / options / death / loading screens.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/MCCore.h"
#include "Items/MCItems.h"

class AMCPlayerController;
class AMCGame;
class AMCPlayer;
class FMCMenu;
class SMCRootWidget;
struct FSlateBrush;

enum class EMCScreen : uint8
{
	None, Title, WorldSelect, CreateWorld, Loading, Playing, Inventory, Container, CreativeInventory, Death, Options, Credits, Disconnected
};

class SMCRootWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCRootWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AMCPlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Per-frame housekeeping (screen switching, chat lifetime, animations). */
	void TickWidget(float DeltaTime);
	void SetPaused(bool bPaused);
	void SetDebugVisible(bool bShow);
	/** Opens the chat / command input. */
	void OpenChat(bool bCommand);
	void CloseChat();
	/** Pause overlay -> full options screen (and back). */
	void OpenOptions();
	void CloseOptions();

	bool IsChatOpen() const { return bChatOpen; }
	/** True while a text box (chat, creative search, world name) owns the keyboard. */
	bool IsTextInputFocused() const;
	FString TakeChatInput();
	/** Widget that should own keyboard focus while a menu is open (the chat entry when typing). */
	TSharedPtr<SWidget> GetFocusTarget();
	/** Inventory hot keys over the hovered slot; return true when consumed. */
	bool HotbarKeyOverSlot(int32 HotbarKey);
	bool ThrowOverSlot(bool bWholeStack);

	AMCPlayerController* GetController() const { return Controller.Get(); }

private:
	TWeakObjectPtr<AMCPlayerController> Controller;
	EMCScreen Screen = EMCScreen::Title;
	TSharedPtr<class SOverlay> ScreenHost;
	TSharedPtr<class SOverlay> ChatBox;
	TSharedPtr<class SOverlay> DebugBox;
	bool bCreative = false;
	bool bShowOptions = false;
	TArray<FString> SuggestionsFor(const FString& Input);
	AMCGame* GameImpl() const;
	float AnimTime = 0.f;
	float FadeAmount = 1.f;
	bool bDebug = false;
	bool bPaused = false;
	bool bChatOpen = false;
	bool bChatCommand = false;
	FString ChatInput;
	FString SearchText;
	bool bSearchDirty = false;          // creative search box changed -> rebuild the palette on the next tick
	void ApplySearch();
	FString SearchApplied;
	TSharedPtr<class SEditableTextBox> CreativeSearch;
	int32 CreativeTab = 2;              // Natural by default
	int32 WorldSelectIndex = -1;
	TArray<FString> WorldList;
	FString NewWorldName = TEXT("New World");
	FString NewWorldSeed;
	int32 NewWorldMode = 1;
	bool bNewWorldCheats = true;
	float OptionsScroll = 0.f;
	int32 SlotFocus = -1;               // menu slot under the cursor
	int32 HoverItem = 0;                // creative palette item under the cursor (tooltip)
	int32 DragButton = -1;
	TArray<int32> DragSlots;
	double LastClickTime = 0.0;
	int32 LastClickSlot = -1;
	int32 ContainerScroll = 0;
	TArray<FString> Suggestions;
	int32 SuggestionIndex = 0;
	FString SuggestionText;

	// ---- chat pieces (the entry is built once per opening so it keeps keyboard focus)
	TSharedPtr<class SVerticalBox> ChatLines;
	TSharedPtr<class SEditableTextBox> ChatEntry;
	TSharedPtr<class SWrapBox> ChatSuggest;
	int32 ChatLinesShown = -1;
	double ChatRefreshTime = 0.0;
	double ChatOpenTime = 0.0;
	bool bChatRebuildPending = false;
	TArray<FString> ChatHistory;
	int32 ChatHistoryPos = -1;
	void RebuildChat();
	void RefreshChatLines();
	void RefreshSuggestions();
	void SubmitChat(const FString& Text);
	FReply OnChatKey(const FGeometry& Geo, const FKeyEvent& Ev);

	// ---- live refresh signatures
	uint32 HudSignature = 0;
	uint32 MenuSignature = 0;
	double DebugRefreshTime = 0.0;
	uint32 ComputeHudSignature() const;
	uint32 ComputeMenuSignature() const;

	// ---- helpers
	AMCGame* Game() const;
	AMCPlayer* Player() const;
	FMCMenu* Menu() const;
	void SyncScreen();
	float GuiScaleFactor() const;
	FMCItemStack StackInSlot(int32 SlotIndex) const;
	FString HoverTooltip() const;
	/** Centres Content, scales it to the GUI scale and adds the dim backdrop + carried-stack / tooltip layer. */
	TSharedRef<SWidget> WrapMenu(TSharedRef<SWidget> Content);
	/** Menu slots + titles + station widgets at their panel coordinates. */
	TSharedRef<SWidget> MakeMenuPanel(FMCMenu* M);

	// ---- widgets
	TSharedRef<SWidget> MakeHUD();
	TSharedRef<SWidget> MakeChat();
	TSharedRef<SWidget> MakeDebugOverlay();
	TSharedRef<SWidget> MakeHotbar();
	TSharedRef<SWidget> MakeStatusBars();
	TSharedRef<SWidget> MakeBossBars();
	TSharedRef<SWidget> MakeInventoryScreen();
	TSharedRef<SWidget> MakeCreativeScreen();
	TSharedRef<SWidget> MakeContainerScreen();
	TSharedRef<SWidget> MakeTitleScreen();
	TSharedRef<SWidget> MakeWorldSelectScreen();
	TSharedRef<SWidget> MakeCreateWorldScreen();
	TSharedRef<SWidget> MakeOptionsScreen();
	TSharedRef<SWidget> MakePauseOverlay();
	TSharedRef<SWidget> MakeDeathScreen();
	TSharedRef<SWidget> MakeCreditsScreen();
	TSharedRef<SWidget> MakeLoadingScreen();

	// ---- building blocks
	TSharedRef<SWidget> Icon(int32 ItemId, float Size);
	TSharedRef<SWidget> Glyph(const TCHAR* Name, float Size);
	TSharedRef<SWidget> Slot(int32 SlotIndex, float Size);
	/** Creative palette cell: renders the item itself and hands it to the carried stack on click. */
	TSharedRef<SWidget> CreativeCell(int32 ItemId, float Size);
	TSharedRef<SWidget> ButtonText(const FString& Label, const FOnClicked& OnClick, float Width = 200.f, bool bDisabled = false);
	TSharedRef<SWidget> Slider(const FString& Label, float Value, float Min, float Max, TFunction<void(float)> OnChange);
	TSharedRef<SWidget> Toggle(const FString& Label, bool bValue, TFunction<void(bool)> OnChange);
	TSharedRef<SWidget> ScreenPanel(const FString& Title, TSharedRef<SWidget> Body, float Width, float Height);

	FReply OnSlotClick(int32 SlotIndex, int32 Button);
	void OnCreativeClick(int32 ItemId, int32 Button);
};
