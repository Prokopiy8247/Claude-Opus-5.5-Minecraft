// Slate UI root: screen switching, HUD, chat, debug overlay (the menu screens live in MCRootWidgetScreens.cpp).
#include "UI/MCRootWidget.h"
#include "UI/MCUI.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SCanvas.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCGameMode.h"
#include "Game/MCMob.h"
#include "Game/MCMenu.h"
#include "Gen/MCBiomes.h"
#include "Render/MCIcons.h"
#include "Render/MCVoxelRenderer.h"
#include "Render/MCParticles.h"
#include "Audio/MCAudio.h"
#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "Opus55MC"

void SMCRootWidget::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	WorldList = [this]() { const UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr; return GI ? GI->ListWorlds() : TArray<FString>(); }();
	// the hosts are overlays: SyncScreen swaps the whole child list, which a box cannot do
	ScreenHost = SNew(SOverlay);
	ChatBox = SNew(SOverlay);
	DebugBox = SNew(SOverlay);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[ ScreenHost.ToSharedRef() ]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top)
		[ DebugBox.ToSharedRef() ]
		// chat: bottom-left, its own slot padding keeps it above the hotbar and status bars
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[ ChatBox.ToSharedRef() ]
	];
	SyncScreen();
}

AMCGame* SMCRootWidget::GameImpl() const { return Game(); }

void SMCRootWidget::TickWidget(float DeltaTime)
{
	AnimTime += DeltaTime;
	AMCGame* G = Game();
	AMCPlayer* P = Player();
	if (!G) return;

	// ---- screen state machine (the game mode decides between the creative and the survival inventory, and it
	// can change while a screen is open, e.g. /gamemode typed with the inventory up)
	const bool bModeChanged = P && !G->bTitleScreen && bCreative != P->IsCreative();
	if (bModeChanged) bCreative = P->IsCreative();
	EMCScreen Want = Screen;
	if (G->bTitleScreen) Want = (Screen == EMCScreen::WorldSelect || Screen == EMCScreen::CreateWorld || Screen == EMCScreen::Options) ? Screen : EMCScreen::Title;
	else if (G->bLoading) Want = EMCScreen::Loading;
	else if (G->bShowCredits) Want = EMCScreen::Credits;
	else if (P && P->bDeadScreen) Want = EMCScreen::Death;
	else if (P && P->ActiveMenu())
	{
		if (P->ActiveMenu()->Type == EMCMenuType::Inventory && bCreative) Want = EMCScreen::CreativeInventory;
		else if (P->ActiveMenu()->Type == EMCMenuType::Inventory) Want = EMCScreen::Inventory;
		else Want = EMCScreen::Container;
	}
	else Want = EMCScreen::Playing;
	// the Options screen can be opened from the pause menu
	if (bPaused && !G->bTitleScreen && bShowOptions) Want = EMCScreen::Options;

	if (Want != Screen || bModeChanged)
	{
		Screen = Want;
		SyncScreen();
	}
	if (bPaused != G->bPaused) { bPaused = G->bPaused; if (bPaused) bShowOptions = false; SyncScreen(); }
	if (bDebug != (Controller.IsValid() && Controller->bShowDebug))
	{
		bDebug = Controller.IsValid() && Controller->bShowDebug;
		SyncScreen();
	}

	// ---- live HUD / menu refresh: rebuild only when the displayed values change
	const double Now = FPlatformTime::Seconds();
	if (bSearchDirty) ApplySearch();
	if (Screen == EMCScreen::Playing)
	{
		const uint32 Sig = ComputeHudSignature();
		if (Sig != HudSignature) { HudSignature = Sig; SyncScreen(); }
	}
	else if (Screen == EMCScreen::Container || Screen == EMCScreen::Loading)
	{
		const uint32 Sig = ComputeMenuSignature();
		if (Sig != MenuSignature) { MenuSignature = Sig; SyncScreen(); }
	}
	if (bDebug && DebugBox.IsValid() && Now - DebugRefreshTime > 0.25)
	{
		DebugRefreshTime = Now;
		DebugBox->ClearChildren();
		DebugBox->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top)[ MakeDebugOverlay() ];
	}

	// ---- chat: the entry is created once per opening (keeps keyboard focus); the lines fade out after 10 s
	const double LastChat = G->ChatTimes.Num() > 0 ? G->ChatTimes.Last() : 0.0;
	const bool bFreshLines = !G->ChatLog.IsEmpty() && Now - LastChat < 10.0;
	const bool bWantBox = bChatOpen || bFreshLines;
	if (bChatRebuildPending || bWantBox != ChatLines.IsValid() || bChatOpen != ChatEntry.IsValid())
	{
		bChatRebuildPending = false;
		RebuildChat();
	}
	else if (ChatLines.IsValid() && (G->ChatLog.Num() != ChatLinesShown || Now - ChatRefreshTime > 0.2))
	{
		RefreshChatLines();
	}
	if (bChatOpen && ChatEntry.IsValid() && !ChatEntry->HasKeyboardFocus() && !ChatEntry->HasFocusedDescendants() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().SetKeyboardFocus(ChatEntry, EFocusCause::SetDirectly);
}

bool SMCRootWidget::IsTextInputFocused() const
{
	if (!FSlateApplication::IsInitialized()) return false;
	const TSharedPtr<SWidget> F = FSlateApplication::Get().GetKeyboardFocusedWidget();
	return F.IsValid() && (F->GetType() == FName(TEXT("SEditableText")) || F->GetType() == FName(TEXT("SEditableTextBox")));
}

uint32 SMCRootWidget::ComputeHudSignature() const
{
	AMCPlayer* P = Player();
	AMCGame* G = Game();
	if (!P || !G) return 0;
	uint32 H = GetTypeHash(P->Selected);
	H = HashCombine(H, GetTypeHash(FMath::CeilToInt(P->Health)));
	H = HashCombine(H, GetTypeHash(FMath::CeilToInt(P->Absorption)));
	H = HashCombine(H, GetTypeHash(P->FoodLevel));
	H = HashCombine(H, GetTypeHash(P->GetArmorValue()));
	H = HashCombine(H, GetTypeHash(P->XPLevel));
	H = HashCombine(H, GetTypeHash(FMath::RoundToInt(P->XPProgress * 90.f)));
	H = HashCombine(H, GetTypeHash(P->AirSupply < P->MaxAir ? P->AirSupply / 30 : -1));
	H = HashCombine(H, GetTypeHash(P->IsCreative()));
	H = HashCombine(H, GetTypeHash(FMath::CeilToInt(P->GetMaxHealth())));
	H = HashCombine(H, GetTypeHash((P->HasEffect(EMCEffect::Wither) ? 1 : 0) | (P->HasEffect(EMCEffect::Poison) ? 2 : 0)));
	const double Now = FPlatformTime::Seconds();
	H = HashCombine(H, GetTypeHash(Now < G->ActionBarTime ? G->ActionBarText : FString()));
	H = HashCombine(H, GetTypeHash(Now < G->TitleTime ? G->TitleText + G->SubtitleText : FString()));
	TArray<AMCMob*> Bosses;
	G->GetBossBars(Bosses);
	for (const AMCMob* M : Bosses) if (M) H = HashCombine(H, GetTypeHash(FMath::CeilToInt(M->Health)));
	return H == 0 ? 1 : H;
}

uint32 SMCRootWidget::ComputeMenuSignature() const
{
	AMCGame* G = Game();
	FMCMenu* M = Menu();
	uint32 H = 7;
	if (G && Screen == EMCScreen::Loading) H = HashCombine(H, GetTypeHash(FMath::RoundToInt(G->LoadingProgress * 50.f)));
	if (!M) return H;
	// station readouts that change the widget structure (recipe lists, enchant offers, beacon level)
	for (int32 i = 0; i < 14; ++i) H = HashCombine(H, GetTypeHash(M->GetData(i)));
	for (int32 i = 0; i < 3; ++i) H = HashCombine(H, GetTypeHash(M->GetText(i)));
	return H;
}

void SMCRootWidget::ApplySearch()
{
	bSearchDirty = false;
	if (SearchText == SearchApplied || Screen != EMCScreen::CreativeInventory) return;
	SearchApplied = SearchText;
	// rebuilding the palette recreates the search box: remember that it had focus and give it back
	SyncScreen();
	if (FSlateApplication::IsInitialized() && CreativeSearch.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(CreativeSearch, EFocusCause::SetDirectly);
		CreativeSearch->GoTo(ETextLocation::EndOfDocument);
	}
}

TSharedPtr<SWidget> SMCRootWidget::GetFocusTarget()
{
	if (bChatOpen && ChatEntry.IsValid()) return ChatEntry;
	return SharedThis(this);
}

void SMCRootWidget::SetPaused(bool InPaused)
{
	bPaused = InPaused;
	if (InPaused) bShowOptions = false;
	SyncScreen();
}

void SMCRootWidget::SetDebugVisible(bool bShow)
{
	if (bDebug == bShow) return;
	bDebug = bShow;
	SyncScreen();
}

void SMCRootWidget::OpenChat(bool bCommand)
{
	bChatOpen = true;
	bChatCommand = bCommand;
	ChatInput = bCommand ? TEXT("/") : FString();
	ChatOpenTime = FPlatformTime::Seconds();
	ChatHistoryPos = -1;
	Suggestions = SuggestionsFor(ChatInput);
	RebuildChat();
	SyncScreen();
}

void SMCRootWidget::CloseChat()
{
	if (!bChatOpen) return;
	bChatOpen = false;
	bChatCommand = false;
	ChatInput.Reset();
	Suggestions.Reset();
	// the entry may be inside its own commit callback: drop it on the next tick
	bChatRebuildPending = true;
	SyncScreen();
}

void SMCRootWidget::OpenOptions()
{
	bShowOptions = true;
	Screen = EMCScreen::Options;
	SyncScreen();
}

void SMCRootWidget::CloseOptions()
{
	bShowOptions = false;
	Screen = EMCScreen::Playing;
	SyncScreen();
}

FString SMCRootWidget::TakeChatInput()
{
	FString Out = ChatInput;
	ChatInput.Reset();
	bChatOpen = false;
	SyncScreen();
	return Out;
}

void SMCRootWidget::SyncScreen()
{
	if (!ScreenHost.IsValid()) return;
	ScreenHost->ClearChildren();
	switch (Screen)
	{
	case EMCScreen::Title: ScreenHost->AddSlot()[ MakeTitleScreen() ]; break;
	case EMCScreen::WorldSelect: ScreenHost->AddSlot()[ MakeWorldSelectScreen() ]; break;
	case EMCScreen::CreateWorld: ScreenHost->AddSlot()[ MakeCreateWorldScreen() ]; break;
	case EMCScreen::Options: ScreenHost->AddSlot()[ MakeOptionsScreen() ]; break;
	case EMCScreen::Loading: ScreenHost->AddSlot()[ MakeLoadingScreen() ]; break;
	case EMCScreen::Death: ScreenHost->AddSlot()[ MakeDeathScreen() ]; break;
	case EMCScreen::Credits: ScreenHost->AddSlot()[ MakeCreditsScreen() ]; break;
	case EMCScreen::Inventory: ScreenHost->AddSlot()[ MakeInventoryScreen() ]; break;
	case EMCScreen::CreativeInventory: ScreenHost->AddSlot()[ MakeCreativeScreen() ]; break;
	case EMCScreen::Container: ScreenHost->AddSlot()[ MakeContainerScreen() ]; break;
	default: ScreenHost->AddSlot()[ MakeHUD() ]; break;
	}
	if (DebugBox.IsValid())
	{
		DebugBox->ClearChildren();
		if (bDebug) DebugBox->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top)[ MakeDebugOverlay() ];
	}
	// the pause overlay floats above the active screen
	if (bPaused && !bChatOpen && Screen == EMCScreen::Playing) ScreenHost->AddSlot()[ MakePauseOverlay() ];
}

// ---------------------------------------------------------------------------------------------------------------------
// HUD

TSharedRef<SWidget> SMCRootWidget::MakeHUD()
{
	AMCPlayer* P = Player();
	if (!P) return SNew(SBox);
	// Minecraft HUD in GUI pixels (hotbar 182x22, 8 px heart pitch...), magnified by the GUI scale like the menus
	const float G = GuiScaleFactor();

	TSharedRef<SVerticalBox> Top = SNew(SVerticalBox);
	TArray<AMCMob*> Bosses;
	if (AMCGame* Gm = Game()) Gm->GetBossBars(Bosses);
	if (Bosses.Num() > 0) Top->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 0.f))[ MakeBossBars() ];
	TSharedRef<SVerticalBox> Middle = SNew(SVerticalBox);
	TSharedRef<SVerticalBox> Bottom = SNew(SVerticalBox);
	if (AMCGame* Gm = Game())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < Gm->TitleTime && !Gm->TitleText.IsEmpty())
		{
			// titles are drawn at 4x / 2x the GUI font like Minecraft's /title
			Middle->AddSlot().AutoHeight().HAlign(HAlign_Center)
			[ SNew(STextBlock).Text(FText::FromString(Gm->TitleText)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 24)).ColorAndOpacity(FSlateColor(MCUI::Gold))
				.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f)) ];
			Middle->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 3.f))
			[ SNew(STextBlock).Text(FText::FromString(Gm->SubtitleText)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)).ColorAndOpacity(FSlateColor(MCUI::Text))
				.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f)) ];
		}
		if (Now < Gm->ActionBarTime && !Gm->ActionBarText.IsEmpty())
			Bottom->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))
			[ SNew(STextBlock).Text(FText::FromString(Gm->ActionBarText)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 6)).ColorAndOpacity(FSlateColor(MCUI::Text))
				.ShadowOffset(FVector2D(0.5f, 0.5f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f)) ];
	}
	Bottom->AddSlot().AutoHeight().HAlign(HAlign_Center)[ MakeStatusBars() ];
	Bottom->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 1.f, 0.f, 0.f))[ MakeHotbar() ];

	return SNew(SDPIScaler).DPIScale(G)
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)[ Top ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ Glyph(TEXT("crosshair"), 15.f) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 60.f))[ Middle ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)[ Bottom ]
	];
}

TSharedRef<SWidget> SMCRootWidget::MakeHotbar()
{
	AMCPlayer* P = Player();
	if (!P) return SNew(SBox);
	// 182x22: nine 20 px cells on a dark strip, the selected one framed (24x24) like Minecraft's
	TSharedRef<SCanvas> C = SNew(SCanvas);
	C->AddSlot().Position(FVector2D(0.f, 0.f)).Size(FVector2D(182.f, 22.f))
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.07f, 0.75f)).Padding(FMargin(1.f))
		[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.22f, 0.55f)) ]
	];
	for (int32 i = 0; i < 9; ++i)
		C->AddSlot().Position(FVector2D(1.f + i * 20.f, 1.f)).Size(FVector2D(20.f, 20.f))[ Slot(i, 20.f) ];
	// selection frame: four 1.5 px bars around the selected cell (an SBorder would fill the whole cell)
	{
		const float X0 = -1.f + P->Selected * 20.f, Y0 = -1.f, W = 24.f, T = 1.5f;
		const FLinearColor Frame(0.96f, 0.96f, 0.96f, 1.f);
		const FVector2D Pos[4] = { FVector2D(X0, Y0), FVector2D(X0, Y0 + W - T), FVector2D(X0, Y0), FVector2D(X0 + W - T, Y0) };
		const FVector2D Size[4] = { FVector2D(W, T), FVector2D(W, T), FVector2D(T, W), FVector2D(T, W) };
		for (int32 k = 0; k < 4; ++k)
			C->AddSlot().Position(Pos[k]).Size(Size[k])
			[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(Frame).Visibility(EVisibility::HitTestInvisible) ];
	}
	// offhand slot to the left of the bar
	if (!P->Inventory.Slots[MCInv::Offhand].IsEmpty())
		C->AddSlot().Position(FVector2D(-29.f, 0.f)).Size(FVector2D(22.f, 22.f))
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.07f, 0.75f)).Padding(FMargin(1.f))
			[ Slot(MCInv::Offhand, 20.f) ]
		];
	return SNew(SBox).WidthOverride(182.f).HeightOverride(22.f)[ C ];
}

TSharedRef<SWidget> SMCRootWidget::MakeStatusBars()
{
	AMCPlayer* P = Player();
	if (!P) return SNew(SBox);
	// 182 px wide block above the hotbar: armour over health on the left, air over hunger on the right,
	// the experience bar (182x5) underneath with the level number centred on it
	const bool bSurvival = !P->IsCreative() && !P->IsSpectator();
	TSharedRef<SCanvas> C = SNew(SCanvas);
	const float Row2 = 10.f, Row1 = 0.f, XPY = 21.f;
	auto Put = [&C](const TCHAR* Name, float X, float Y, float S)
	{
		if (const FSlateBrush* B = MCIcons::GetGlyph(FName(Name)))
			C->AddSlot().Position(FVector2D(X, Y)).Size(FVector2D(S, S))[ SNew(SImage).Image(B) ];
	};
	if (bSurvival)
	{
		const int32 MaxHearts = FMath::Min(10, FMath::CeilToInt(P->GetMaxHealth() / 2.f));
		for (int32 i = 0; i < MaxHearts; ++i)
		{
			const float Hp = P->Health - i * 2.f;
			const TCHAR* Gl = Hp >= 2.f ? TEXT("heart") : (Hp > 0.f ? TEXT("heart_half") : TEXT("heart_bg"));
			if (P->HasEffect(EMCEffect::Wither)) Gl = Hp > 0.f ? TEXT("heart_wither") : TEXT("heart_bg");
			else if (P->HasEffect(EMCEffect::Poison)) Gl = Hp >= 2.f ? TEXT("heart_poison") : (Hp > 0.f ? TEXT("heart_half") : TEXT("heart_bg"));
			// low health: the hearts jitter like Minecraft's
			const float Jit = P->Health <= 4.f ? (FMath::Fmod((float)FPlatformTime::Seconds() * 20.f + i * 7.f, 3.f) - 1.f) : 0.f;
			Put(Gl, i * 8.f, Row2 + Jit, 9.f);
		}
		const int32 Abs = FMath::Min(10, FMath::CeilToInt(P->Absorption / 2.f));
		const int32 ArmorPts = P->GetArmorValue();
		for (int32 i = 0; i < Abs; ++i) Put(TEXT("heart_absorb"), i * 8.f, Row1, 9.f);
		if (Abs == 0 && ArmorPts > 0)
			for (int32 i = 0; i < 10; ++i)
				Put(ArmorPts >= (i + 1) * 2 ? TEXT("armor") : (ArmorPts > i * 2 ? TEXT("armor") : TEXT("armor_bg")), i * 8.f, Row1, 9.f);
		for (int32 i = 0; i < 10; ++i)
		{
			const float F = P->FoodLevel - i * 2.f;
			Put(F >= 2.f ? TEXT("hunger") : (F > 0.f ? TEXT("hunger_half") : TEXT("hunger_bg")), 173.f - i * 8.f, Row2, 9.f);
		}
		if (P->AirSupply < P->MaxAir)
		{
			const int32 Count = FMath::CeilToInt(P->AirSupply / (float)FMath::Max(1, P->MaxAir) * 10.f);
			for (int32 i = 0; i < 10; ++i) Put(i < Count ? TEXT("bubble") : TEXT("bubble_burst"), 173.f - i * 8.f, Row1, 9.f);
		}
	}
	if (!P->IsSpectator())
	{
		C->AddSlot().Position(FVector2D(0.f, XPY)).Size(FVector2D(182.f, 5.f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.03f, 0.03f, 0.03f, 0.85f)) ]
			+ SOverlay::Slot().HAlign(HAlign_Left).Padding(FMargin(0.5f))
			[
				SNew(SBox).WidthOverride(FMath::Clamp(P->XPProgress, 0.f, 1.f) * 181.f)
				[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.5f, 1.f, 0.13f, 1.f)) ]
			]
		];
		if (P->XPLevel > 0)
			C->AddSlot().Position(FVector2D(66.f, XPY - 9.f)).Size(FVector2D(50.f, 10.f))
			[
				SNew(SBox).HAlign(HAlign_Center)
				[ SNew(STextBlock).Text(FText::FromString(FString::FromInt(P->XPLevel))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 6))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.f, 0.13f, 1.f))).ShadowOffset(FVector2D(0.6f, 0.6f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 1.f)) ]
			];
	}
	return SNew(SBox).WidthOverride(182.f).HeightOverride(27.f)[ C ];
}

TSharedRef<SWidget> SMCRootWidget::MakeBossBars()
{
	AMCGame* G = Game();
	TArray<AMCMob*> Bosses;
	if (G) G->GetBossBars(Bosses);
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	for (AMCMob* M : Bosses)
	{
		if (!M || !M->Def) continue;
		const FString Title = M->CustomName.IsEmpty() ? M->Def->Name : M->CustomName;
		const float Frac = FMath::Clamp(M->Health / FMath::Max(1.f, M->GetMaxHealth()), 0.f, 1.f);
		const bool bWither = M->Def->Id == TEXT("wither");
		const FLinearColor Bar = bWither ? FLinearColor(0.62f, 0.12f, 0.75f, 1.f) : FLinearColor(0.9f, 0.2f, 0.62f, 1.f);
		Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 1.f))
		[
			SNew(SBox).WidthOverride(182.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[ SNew(STextBlock).Text(FText::FromString(Title)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 6)).ColorAndOpacity(FSlateColor(MCUI::Text))
					.ShadowOffset(FVector2D(0.6f, 0.6f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 1.f)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 1.f))
				[
					SNew(SBox).HeightOverride(5.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(Bar.R * 0.3f, Bar.G * 0.3f, Bar.B * 0.3f, 0.9f)) ]
						+ SOverlay::Slot().HAlign(HAlign_Left)
						[ SNew(SBox).WidthOverride(Frac * 182.f)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(Bar) ] ]
					]
				]
			]
		];
	}
	return Col;
}

// ---------------------------------------------------------------------------------------------------------------------
// Chat

TSharedRef<SWidget> SMCRootWidget::MakeChat()
{
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight().HAlign(HAlign_Left)[ SAssignNew(ChatLines, SVerticalBox) ];
	if (bChatOpen)
	{
		Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.7f))
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.Padding(FMargin(4.f))
			[
				SAssignNew(ChatEntry, SEditableTextBox)
				.Text(FText::FromString(ChatInput))
				.Font(MCUI::FontM())
				.SelectAllTextWhenFocused(false)
				.ClearKeyboardFocusOnCommit(false)
				.RevertTextOnEscape(false)
				.OnKeyDownHandler(FOnKeyDown::CreateSP(this, &SMCRootWidget::OnChatKey))
				.OnTextChanged_Lambda([this](const FText& T)
				{
					FString S = T.ToString();
					// the key that opened the chat (T or /) arrives as a character right after: swallow it
					if (FPlatformTime::Seconds() - ChatOpenTime < 0.25)
					{
						const FString Opener = bChatCommand ? TEXT("//") : TEXT("t");
						if (S.Equals(Opener, ESearchCase::IgnoreCase))
						{
							S = bChatCommand ? TEXT("/") : FString();
							if (ChatEntry.IsValid()) { ChatEntry->SetText(FText::FromString(S)); ChatEntry->GoTo(ETextLocation::EndOfDocument); }
						}
					}
					ChatInput = S;
					Suggestions = SuggestionsFor(ChatInput);
					SuggestionIndex = -1;
					RefreshSuggestions();
				})
				.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type Type)
				{
					if (Type == ETextCommit::OnEnter) SubmitChat(T.ToString());
					else if (Type == ETextCommit::OnCleared)
					{
						CloseChat();
						if (AMCPlayerController* C = Controller.Get()) { C->bChatOpen = false; C->UpdateInputMode(); }
					}
					// focus moves (clicking the world) keep the chat open; TickWidget restores focus
				})
			]
		];
		Col->AddSlot().AutoHeight()[ SAssignNew(ChatSuggest, SWrapBox).UseAllottedSize(true) ];
	}
	return SNew(SBox).WidthOverride(620.f)[ Col ];
}

void SMCRootWidget::RebuildChat()
{
	if (!ChatBox.IsValid()) return;
	ChatBox->ClearChildren();
	ChatLines.Reset();
	ChatEntry.Reset();
	ChatSuggest.Reset();
	AMCGame* G = Game();
	const double LastChat = (G && G->ChatTimes.Num() > 0) ? G->ChatTimes.Last() : 0.0;
	const bool bFresh = G && !G->ChatLog.IsEmpty() && FPlatformTime::Seconds() - LastChat < 10.0;
	if (!bChatOpen && !bFresh) return;
	ChatBox->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(6.f, 0.f, 0.f, bChatOpen ? 6.f : 56.f * GuiScaleFactor()))[ MakeChat() ];
	ChatLinesShown = -1;
	RefreshChatLines();
	RefreshSuggestions();
	if (ChatEntry.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(ChatEntry, EFocusCause::SetDirectly);
		ChatEntry->GoTo(ETextLocation::EndOfDocument);
	}
}

void SMCRootWidget::RefreshChatLines()
{
	if (!ChatLines.IsValid()) return;
	ChatLines->ClearChildren();
	ChatRefreshTime = FPlatformTime::Seconds();
	AMCGame* G = Game();
	if (!G) return;
	ChatLinesShown = G->ChatLog.Num();
	const int32 Start = FMath::Max(0, G->ChatLog.Num() - (bChatOpen ? 14 : 8));
	for (int32 i = Start; i < G->ChatLog.Num(); ++i)
	{
		const double Age = FPlatformTime::Seconds() - (G->ChatTimes.IsValidIndex(i) ? G->ChatTimes[i] : 0.0);
		const float Alpha = bChatOpen ? 1.f : FMath::Clamp(1.f - (float)(Age - 8.0) / 2.0f, 0.f, 1.f);
		if (Alpha <= 0.01f) continue;
		FString Msg = G->ChatLog[i];
		FLinearColor RC = MCUI::Text;
		if (Msg.StartsWith(TEXT("§c"))) { RC = MCUI::Danger; Msg = Msg.Mid(2); }
		else if (Msg.StartsWith(TEXT("§e"))) { RC = MCUI::Gold; Msg = Msg.Mid(2); }
		else if (Msg.StartsWith(TEXT("§a"))) { RC = MCUI::Accent; Msg = Msg.Mid(2); }
		else if (Msg.StartsWith(TEXT("§"))) Msg = Msg.Mid(2);
		RC.A = Alpha;
		ChatLines->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha))
			.Padding(FMargin(4.f, 1.f))
			[
				SNew(STextBlock).Text(FText::FromString(Msg)).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(RC))
				.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f * Alpha))
				.WrapTextAt(600.f)
			]
		];
	}
}

void SMCRootWidget::RefreshSuggestions()
{
	if (!ChatSuggest.IsValid()) return;
	ChatSuggest->ClearChildren();
	if (Suggestions.IsEmpty() || !ChatInput.StartsWith(TEXT("/"))) return;
	for (int32 i = 0; i < FMath::Min(Suggestions.Num(), 14); ++i)
		ChatSuggest->AddSlot().Padding(FMargin(3.f, 1.f))
		[
			SNew(STextBlock).Text(FText::FromString(Suggestions[i])).Font(MCUI::FontS())
			.ColorAndOpacity(FSlateColor(i == SuggestionIndex ? MCUI::Text : MCUI::Gold))
		];
}

FReply SMCRootWidget::OnChatKey(const FGeometry& Geo, const FKeyEvent& Ev)
{
	const FKey K = Ev.GetKey();
	if (K == EKeys::Escape)
	{
		CloseChat();
		if (AMCPlayerController* C = Controller.Get()) { C->bChatOpen = false; C->UpdateInputMode(); }
		return FReply::Handled();
	}
	if (K == EKeys::Tab)
	{
		// cycle through the completions for the last word
		if (Suggestions.Num() > 0 && ChatEntry.IsValid())
		{
			SuggestionIndex = (SuggestionIndex + 1) % Suggestions.Num();
			FString Base = ChatInput;
			int32 Space = INDEX_NONE;
			Base.FindLastChar(TEXT(' '), Space);
			const FString Pick = Suggestions[SuggestionIndex];
			const FString NewText = (Space == INDEX_NONE) ? (Pick.StartsWith(TEXT("/")) ? Pick : TEXT("/") + Pick) : Base.Left(Space + 1) + Pick;
			ChatEntry->SetText(FText::FromString(NewText));
			ChatEntry->GoTo(ETextLocation::EndOfDocument);
			const int32 Keep = SuggestionIndex;
			ChatInput = NewText;
			RefreshSuggestions();
			SuggestionIndex = Keep;
		}
		return FReply::Handled();
	}
	if ((K == EKeys::Up || K == EKeys::Down) && ChatHistory.Num() > 0 && ChatEntry.IsValid())
	{
		if (K == EKeys::Up) ChatHistoryPos = ChatHistoryPos < 0 ? ChatHistory.Num() - 1 : FMath::Max(0, ChatHistoryPos - 1);
		else ChatHistoryPos = ChatHistoryPos < 0 ? -1 : (ChatHistoryPos + 1 < ChatHistory.Num() ? ChatHistoryPos + 1 : -1);
		const FString T = ChatHistoryPos >= 0 ? ChatHistory[ChatHistoryPos] : FString();
		ChatEntry->SetText(FText::FromString(T));
		ChatEntry->GoTo(ETextLocation::EndOfDocument);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMCRootWidget::SubmitChat(const FString& Text)
{
	const FString Cmd = Text.TrimStartAndEnd();
	if (!Cmd.IsEmpty())
	{
		ChatHistory.Add(Cmd);
		if (ChatHistory.Num() > 50) ChatHistory.RemoveAt(0);
	}
	CloseChat();
	if (AMCPlayerController* C = Controller.Get()) { C->bChatOpen = false; C->UpdateInputMode(); }
	if (AMCGame* G = Game())
	{
		if (Cmd.StartsWith(TEXT("/")))
		{
			if (!G->ExecuteCommand(Cmd, Player())) G->AddChat(FString::Printf(TEXT("§cUnknown or invalid command: %s"), *Cmd));
		}
		else if (!Cmd.IsEmpty()) G->AddChat(TEXT("<Steve> ") + Cmd);
	}
}

TArray<FString> SMCRootWidget::SuggestionsFor(const FString& Input)
{
	AMCGame* G = Game();
	if (!G || !Input.StartsWith(TEXT("/"))) return TArray<FString>();
	return G->GetCommandSuggestions(Input);
}

// ---------------------------------------------------------------------------------------------------------------------
// F3 debug overlay

TSharedRef<SWidget> SMCRootWidget::MakeDebugOverlay()
{
	AMCGame* G = Game();
	AMCPlayer* P = Player();
	if (!G) return SNew(SBox);
	const FMCWorld* W = G->ActiveWorld();
	const AMCVoxelRenderer* R = G->Renderer.Get();
	FMCBlockPos Bp;
	if (P) Bp = P->BlockPos();
	const FVector Cam = P ? P->Pos : FVector::ZeroVector;
	const bool bCharts = Controller.IsValid() && Controller->bShowDebugCharts;

	auto Line = [](const FString& Text, const FLinearColor& C)
	{
		return SNew(STextBlock).Text(FText::FromString(Text)).Font(MCUI::FontMono()).ColorAndOpacity(FSlateColor(C))
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	};

	static const TCHAR* DifficultyNames[] = { TEXT("Peaceful"), TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Opus 5.5 Minecraft 1.0  -  %s"), *G->WorldName), MCUI::Accent) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("%d fps   T: %.2f ms   E: %d"), G->FPS, G->AvgTickMs, W ? W->Entities.Num() : 0), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("XYZ: %.3f / %.5f / %.3f"), Cam.X, Cam.Y, Cam.Z), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Block: %d %d %d   Chunk: %d %d"), Bp.X, Bp.Y, Bp.Z, Bp.X >> 4, Bp.Z >> 4), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Facing: %s (%.1f / %.1f)"), MC::FaceName(MC::FaceFromYaw(P ? P->Yaw : 0.f)), P ? P->Yaw : 0.f, P ? P->Pitch : 0.f), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Biome: %s"), W ? *FMCBiomes::Get(W->GetBiome(Bp)).Display : TEXT("-")), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Light: %d (sky %d, block %d)"),
		W ? W->GetLight(Bp, G->GetSkyDarken()) : 0, W ? (int32)W->GetSkyLight(Bp) : 0, W ? (int32)W->GetBlockLight(Bp) : 0), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Day %lld   Time %02d:%02d"), G->DayTime / 24000,
		(int32)(((G->DayTime % 24000) / 1000 + 6) % 24), (int32)(((G->DayTime % 1000) * 60) / 1000)), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Weather: %s   Moon: %d"),
		G->bThundering ? TEXT("thunder") : (G->bRaining ? TEXT("rain") : TEXT("clear")), G->GetMoonPhase()), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Seed: %llu"), G->Seed), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Dimension: %s   Difficulty: %s"),
		G->ActiveDim == EMCDimension::Overworld ? TEXT("Overworld") : (G->ActiveDim == EMCDimension::Nether ? TEXT("Nether") : TEXT("The End")),
		DifficultyNames[FMath::Clamp((int32)G->Difficulty, 0, 3)]), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Chunks: %d loaded, %d pending"), W ? W->Chunks.Num() : 0, W ? W->NumPendingGeneration() : 0), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Meshes: %d (%.1f ms)   Tris: %d"), R ? R->StatComponents : 0, R ? R->StatMeshMs : 0.0, R ? R->StatTriangles : 0), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(FString::Printf(TEXT("Particles: %d   Lights: %d   Sounds/s: %d   Clips: %d"),
		G->Particles ? G->Particles->Num() : 0, R ? R->StatLights : 0, G->Audio ? G->Audio->SoundsThisSecond : 0,
		G->Audio ? G->Audio->GetClipCache().Num() : 0), MCUI::Text) ];
	Rows->AddSlot().AutoHeight()[ Line(TEXT("F3+F4 charts, F1 hide HUD, F2 screenshot, F5 perspective"), MCUI::TextDim) ];

	// ---- optional charts (F3+F4): tick-time bars plus a colour strip of the surrounding biomes
	if (bCharts && W)
	{
		TSharedRef<SHorizontalBox> Bars = SNew(SHorizontalBox);
		const int32 N = FMath::Min(G->TickHistory.Num(), 60);
		for (int32 i = 0; i < N; ++i)
		{
			const double Ms = G->TickHistory[G->TickHistory.Num() - N + i];
			const float Frac = FMath::Clamp((float)Ms / 50.f, 0.f, 1.f);
			const FLinearColor C = FLinearColor::LerpUsingHSV(FLinearColor(0.25f, 0.85f, 0.3f), FLinearColor(0.9f, 0.2f, 0.15f), Frac);
			Bars->AddSlot().AutoWidth().Padding(FMargin(0.5f, 0.f)).VAlign(VAlign_Bottom)
			[
				SNew(SBox).WidthOverride(5.f).HeightOverride(2.f + Frac * 38.f)
				[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(C) ]
			];
		}
		Rows->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))[ Line(FString::Printf(TEXT("Tick history (last %d, 50 ms full scale)"), N), MCUI::TextDim) ];
		Rows->AddSlot().AutoHeight()[ Bars ];

		// biome strip along the Z axis beneath / above the player
		TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
		for (int32 d = -6; d <= 6; ++d)
		{
			const uint8 B = W->GetBiome(FMCBlockPos(Bp.X, Bp.Y, Bp.Z + d * 4));
			Strip->AddSlot().AutoWidth()[ SNew(SBox).WidthOverride(14.f).HeightOverride(10.f)
				[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(FMCBiomes::Get(B).Grass)) ] ];
		}
		Rows->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))[ Line(TEXT("Biomes below / above (4 blocks per step)"), MCUI::TextDim) ];
		Rows->AddSlot().AutoHeight()[ Strip ];
	}

	return SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(4.f))
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		.Padding(FMargin(6.f))
		[ Rows ]
	];
}

#undef LOCTEXT_NAMESPACE
