// Slate screens: inventory + creative tabs with search, container screens (station panels), title / world list /
// create world / options / pause / death / credits / loading.
#include "UI/MCRootWidget.h"
#include "UI/MCUI.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCGameMode.h"
#include "Game/MCMenu.h"
#include "Game/MCMob.h"
#include "Render/MCIcons.h"
#include "Items/MCLoot.h"
#include "World/MCBlockEntity.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Audio/MCAudio.h"

namespace
{
	const float SlotStep = 20.f;   // slot grid pitch (18 px slot + 2 px gap)
	const float SlotSize = 18.f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Player inventory (2x2 crafting, armour, offhand) and container screens (chest, furnace, brewing, anvil, enchanting...)

TSharedRef<SWidget> SMCRootWidget::MakeInventoryScreen()
{
	FMCMenu* M = Menu();
	if (!M) return SNew(SBox);
	return WrapMenu(MakeMenuPanel(M));
}

TSharedRef<SWidget> SMCRootWidget::MakeContainerScreen()
{
	FMCMenu* M = Menu();
	if (!M) return SNew(SBox);
	return WrapMenu(MakeMenuPanel(M));
}

TSharedRef<SWidget> SMCRootWidget::MakeCreativeScreen()
{
	AMCPlayer* P = Player();
	FMCMenu* M = Menu();
	if (!P || !M) return SNew(SBox);

	// Minecraft 1.20+ layout in GUI pixels: a 195x136 panel, 7 tab slots above it and 7 below, each tab shows an item
	struct FTabDef { int32 Tab; int32 Column; bool bTop; const TCHAR* Title; const TCHAR* Icon; };
	static const FTabDef Tabs[] = {
		{ 0, 0, true, TEXT("Building Blocks"), TEXT("bricks") }, { 1, 1, true, TEXT("Colored Blocks"), TEXT("cyan_wool") },
		{ 2, 2, true, TEXT("Natural Blocks"), TEXT("grass_block") }, { 3, 3, true, TEXT("Functional Blocks"), TEXT("crafting_table") },
		{ 4, 4, true, TEXT("Redstone Blocks"), TEXT("redstone") }, { 10, 6, true, TEXT("Search Items"), TEXT("compass") },
		{ 5, 0, false, TEXT("Tools & Utilities"), TEXT("diamond_pickaxe") }, { 6, 1, false, TEXT("Combat"), TEXT("golden_sword") },
		{ 7, 2, false, TEXT("Food & Drinks"), TEXT("golden_apple") }, { 8, 3, false, TEXT("Ingredients"), TEXT("iron_ingot") },
		{ 9, 4, false, TEXT("Spawn Eggs"), TEXT("pig_spawn_egg") }, { 11, 6, false, TEXT("Survival Inventory"), TEXT("chest") } };
	static const EMCTab TabIds[] = { EMCTab::Building, EMCTab::Colored, EMCTab::Natural, EMCTab::Functional, EMCTab::Redstone, EMCTab::Tools,
		EMCTab::Combat, EMCTab::Food, EMCTab::Ingredients, EMCTab::SpawnEggs };
	const float PanelW = 195.f, PanelH = 136.f, TabW = 26.f, TabH = 28.f, Step = 18.f;
	const float PanelY = TabH - 3.f;
	const float BottomY = PanelY + PanelH - 3.f;
	CreativeTab = FMath::Clamp(CreativeTab, 0, 11);
	const bool bSearchTab = CreativeTab == 10;
	const bool bInvTab = CreativeTab == 11;

	TSharedRef<SCanvas> C = SNew(SCanvas);
	auto Box = [](const FLinearColor& Col) { return SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(Col); };
	const FLinearColor Frame(0.78f, 0.78f, 0.80f, 1.f), Fill(0.42f, 0.42f, 0.45f, 0.98f), TabIdle(0.30f, 0.30f, 0.33f, 0.98f);
	auto AddTab = [&](const FTabDef& T, bool bSelectedPass)
	{
		const bool bSel = T.Tab == CreativeTab;
		if (bSel != bSelectedPass) return;
		// columns 0-4 from the left edge, column 6 flush with the right edge (Minecraft keeps search / inventory there)
		const float X = T.Column == 6 ? PanelW - TabW : T.Column * (TabW + 2.f);
		const float Y = T.bTop ? (bSel ? 0.f : 2.f) : BottomY;
		const float H = bSel ? TabH + 1.f : TabH - 2.f;
		const FMCItemStack Icon = FMCItemStack::Of(T.Icon, 1);
		const int32 TabIndex = T.Tab;
		C->AddSlot().Position(FVector2D(X, Y)).Size(FVector2D(TabW, H))
		[
			SNew(SBox).ToolTipText(FText::FromString(T.Title))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ Box(bSel ? Frame : FLinearColor(0.55f, 0.55f, 0.58f, 1.f)) ]
				+ SOverlay::Slot().Padding(FMargin(1.f, T.bTop ? 1.f : 0.f, 1.f, T.bTop ? 0.f : 1.f))[ Box(bSel ? Fill : TabIdle) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[ SNew(SMCItemStackWidget).Size(16.f).Getter([Icon]() { return Icon; }).Visibility(EVisibility::HitTestInvisible) ]
				+ SOverlay::Slot()
				[
					SNew(SMCSlotHit).OnClick([this, TabIndex](int32)
					{
						if (CreativeTab == TabIndex) return;
						CreativeTab = TabIndex;
						if (TabIndex != 10) SearchText.Reset();
						SearchApplied = SearchText;
						SyncScreen();
					})
				]
			]
		];
	};
	// unselected tabs sit behind the panel, the selected one is drawn on top of it (joined to the panel)
	for (const FTabDef& T : Tabs) AddTab(T, false);
	C->AddSlot().Position(FVector2D(0.f, PanelY)).Size(FVector2D(PanelW, PanelH))
	[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(Frame).Padding(FMargin(1.f))[ Box(Fill) ] ];
	for (const FTabDef& T : Tabs) AddTab(T, true);

	// title
	const TCHAR* Title = TEXT("");
	for (const FTabDef& T : Tabs) if (T.Tab == CreativeTab) Title = T.Title;
	C->AddSlot().Position(FVector2D(8.f, PanelY + 5.f)).Size(FVector2D(90.f, 10.f))
	[ SNew(STextBlock).Text(FText::FromString(Title)).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.94f, 1.f))) ];

	// the player's slots are addressed through the open inventory menu
	auto MenuIndex = [M, P](int32 InvIndex)
	{
		for (int32 k = 0; k < M->Slots.Num(); ++k)
			if (M->Slots[k].Container == &P->Inventory && M->Slots[k].Index == InvIndex) return k;
		return -1;
	};
	// hotbar row along the bottom of every tab
	for (int32 Col = 0; Col < 9; ++Col)
		C->AddSlot().Position(FVector2D(8.f + Col * Step, PanelY + 111.f)).Size(FVector2D(Step, Step))[ Slot(MenuIndex(Col), Step) ];
	C->AddSlot().Position(FVector2D(8.f + P->Selected * Step, PanelY + 111.f)).Size(FVector2D(Step, Step))
	[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.22f)).Visibility(EVisibility::HitTestInvisible) ];

	if (bInvTab)
	{
		// survival inventory inside the creative frame: armour, offhand, main rows and the destroy slot
		static const int32 ArmorInv[4] = { 39, 38, 37, 36 };
		for (int32 a = 0; a < 4; ++a)
			C->AddSlot().Position(FVector2D(a < 2 ? 53.f : 107.f, PanelY + 5.f + (a % 2) * Step)).Size(FVector2D(Step, Step))[ Slot(MenuIndex(ArmorInv[a]), Step) ];
		C->AddSlot().Position(FVector2D(34.f, PanelY + 19.f)).Size(FVector2D(Step, Step))[ Slot(MenuIndex(MCInv::Offhand), Step) ];
		for (int32 Row = 0; Row < 3; ++Row)
			for (int32 Col = 0; Col < 9; ++Col)
				C->AddSlot().Position(FVector2D(8.f + Col * Step, PanelY + 53.f + Row * Step)).Size(FVector2D(Step, Step))[ Slot(MenuIndex(9 + Row * 9 + Col), Step) ];
		const FMCItemStack Barrier = FMCItemStack::Of(TEXT("barrier"), 1);
		C->AddSlot().Position(FVector2D(172.f, PanelY + 111.f)).Size(FVector2D(Step, Step))
		[
			SNew(SBox).ToolTipText(FText::FromString(TEXT("Destroy Item (shift-click: clear the inventory)")))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ Box(MCUI::SlotBg) ]
				+ SOverlay::Slot().Padding(FMargin(1.f))[ Box(FLinearColor(0.35f, 0.08f, 0.08f, 0.95f)) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[ SNew(SMCItemStackWidget).Size(14.f).Getter([Barrier]() { return Barrier; }).Visibility(EVisibility::HitTestInvisible) ]
				+ SOverlay::Slot()
				[
					SNew(SMCSlotHit).OnClick([this](int32)
					{
						FMCMenu* MM = Menu();
						AMCPlayer* PP = Player();
						if (!MM || !PP) return;
						if (!MM->Carried().IsEmpty()) MM->Carried() = FMCItemStack();
						else if (FSlateApplication::Get().GetModifierKeys().IsShiftDown())
							for (int32 i = 0; i < MCInv::Size; ++i) PP->Inventory.Slots[i] = FMCItemStack();
					})
				]
			]
		];
		return WrapMenu(SNew(SBox).WidthOverride(PanelW).HeightOverride(BottomY + TabH)[ C ]);
	}

	// ---- item palette for the selected tab (or the search results)
	TArray<FMCItemId> Items;
	const FString Needle = SearchText.TrimStartAndEnd().ToLower();
	const FString NeedleId = Needle.Replace(TEXT(" "), TEXT("_"));
	for (const FMCItem& I : FMCItems::All())
	{
		if (I.Id == 0 || I.bHidden || I.Tab == EMCTab::OpOnly) continue;
		if (bSearchTab)
		{
			if (!Needle.IsEmpty() && !I.DisplayName.ToLower().Contains(Needle) && !I.Name.ToString().Contains(NeedleId)) continue;
		}
		else if (I.Tab != TabIds[CreativeTab]) continue;
		Items.Add(I.Id);
	}
	const int32 PerRow = 9;
	const int32 Rows = FMath::Max(5, FMath::DivideAndRoundUp(Items.Num(), PerRow));
	TSharedRef<SCanvas> Grid = SNew(SCanvas);
	for (int32 i = 0; i < Rows * PerRow; ++i)
	{
		const FVector2D At((i % PerRow) * Step, (i / PerRow) * Step);
		if (Items.IsValidIndex(i)) Grid->AddSlot().Position(At).Size(FVector2D(Step, Step))[ CreativeCell((int32)Items[i], Step) ];
		else Grid->AddSlot().Position(At).Size(FVector2D(Step, Step))[ SNew(SBox).Padding(FMargin(1.f))[ Box(FLinearColor(0.13f, 0.13f, 0.14f, 0.95f)) ] ];
	}
	TSharedRef<SScrollBox> Scroll = SNew(SScrollBox).ScrollBarThickness(FVector2D(4.f, 4.f)).ScrollBarAlwaysVisible(Rows > 5);
	Scroll->AddSlot()[ SNew(SBox).WidthOverride(PerRow * Step).HeightOverride(Rows * Step)[ Grid ] ];
	C->AddSlot().Position(FVector2D(8.f, PanelY + 17.f)).Size(FVector2D(PerRow * Step + 10.f, 5.f * Step))[ Scroll ];

	if (bSearchTab)
	{
		C->AddSlot().Position(FVector2D(96.f, PanelY + 3.f)).Size(FVector2D(92.f, 13.f))
		[
			SAssignNew(CreativeSearch, SEditableTextBox)
			.Text(FText::FromString(SearchText))
			.HintText(FText::FromString(TEXT("Search...")))
			.Font(MCUI::FontPanel())
			.Padding(FMargin(2.f, 0.f))
			.OnTextChanged_Lambda([this](const FText& T) { SearchText = T.ToString(); bSearchDirty = true; })
		];
	}
	return WrapMenu(SNew(SBox).WidthOverride(PanelW).HeightOverride(BottomY + TabH)[ C ]);
}

// ---------------------------------------------------------------------------------------------------------------------
// Title / world selection / create / options / pause / death / credits / loading
TSharedRef<SWidget> SMCRootWidget::MakeTitleScreen()
{
	AMCGame* G = Game();
	const FString Version = TEXT("Opus 5.5 Minecraft 1.0 - original recreation");
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().FillHeight(0.35f)[ SNullWidget::NullWidget ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("OPUS 5.5 MINECRAFT"))).Font(MCUI::FontXL())
		.ColorAndOpacity(FSlateColor(MCUI::Text)).ShadowOffset(FVector2D(3.f, 3.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f)) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 14.f))
	[ SNew(STextBlock).Text(FText::FromString(Version)).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::Gold)) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Singleplayer"), FOnClicked::CreateLambda([this]()
	{
		if (UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr)
		{
			WorldList = GI->ListWorlds();
			for (const FString& W : WorldList) if (W == GI->PendingWorld) WorldSelectIndex = WorldList.IndexOfByKey(W);
		}
		Screen = EMCScreen::WorldSelect;
		SyncScreen();
		return FReply::Handled();
	}), 320.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Create New World"), FOnClicked::CreateLambda([this]()
	{
		Screen = EMCScreen::CreateWorld;
		SyncScreen();
		return FReply::Handled();
	}), 320.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Options..."), FOnClicked::CreateLambda([this]()
	{
		Screen = EMCScreen::Options;
		SyncScreen();
		return FReply::Handled();
	}), 320.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Quit Game"), FOnClicked::CreateLambda([this]()
	{
		UKismetSystemLibrary::QuitGame(Player(), Controller.Get(), EQuitPreference::Quit, false);
		return FReply::Handled();
	}), 320.f) ];
	Col->AddSlot().FillHeight(0.65f)[ SNullWidget::NullWidget ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
	[ SNew(STextBlock).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim))
		.Text(FText::FromString(G && G->Renderer ? FString::Printf(TEXT("Procedural world preview - seed %llu"), G->Seed) : TEXT(""))) ];
	return Col;
}

TSharedRef<SWidget> SMCRootWidget::MakeWorldSelectScreen()
{
	UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr;
	if (GI) WorldList = GI->ListWorlds();
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 i = 0; i < WorldList.Num(); ++i)
	{
		const bool bSel = (i == WorldSelectIndex);
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SNew(SButton)
			.ButtonColorAndOpacity(bSel ? MCUI::Accent : FLinearColor(0.3f, 0.3f, 0.33f))
			.ContentPadding(FMargin(10.f, 8.f))
			.OnClicked(FOnClicked::CreateLambda([this, i]() { WorldSelectIndex = i; SyncScreen(); return FReply::Handled(); }))
			[ SNew(STextBlock).Text(FText::FromString(WorldList[i])).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(bSel ? FLinearColor::Black : MCUI::Text)) ]
		];
	}
	if (WorldList.Num() == 0) List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 20.f))
		[ SNew(STextBlock).Text(FText::FromString(TEXT("No worlds yet - create one first"))).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ];

	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("Select World"))).Font(MCUI::FontXL()).ColorAndOpacity(FSlateColor(MCUI::Text)) ];
	Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ SNew(SBox).HeightOverride(300.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ] ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Play Selected World"), FOnClicked::CreateLambda([this]() { if (AMCPlayerController* C = Controller.Get()) { if (UMCGameInstance* GI2 = Cast<UMCGameInstance>(C->GetGameInstance())) { GI2->PendingWorld = WorldList.IsValidIndex(WorldSelectIndex) ? WorldList[WorldSelectIndex] : FString(); GI2->bPendingLoad = true; GI2->PendingMode = 1; } } if (AMCGame* G = Game()) { G->StartWorld(WorldList.IsValidIndex(WorldSelectIndex) ? WorldList[WorldSelectIndex] : TEXT("New World"), 0, true, EMCGameMode::Creative); Screen = EMCScreen::Playing; SyncScreen(); } return FReply::Handled(); }), 200.f) ]
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Create New"), FOnClicked::CreateLambda([this]() { Screen = EMCScreen::CreateWorld; SyncScreen(); return FReply::Handled(); }), 150.f) ]
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Delete"), FOnClicked::CreateLambda([this]()
		{
			if (!WorldList.IsValidIndex(WorldSelectIndex)) return FReply::Handled();
			if (UMCGameInstance* GI3 = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr)
			{
				const FString Dir = GI3->SaveRoot() / WorldList[WorldSelectIndex];
				IFileManager::Get().DeleteDirectory(*Dir, false, true);
				WorldList = GI3->ListWorlds();
				WorldSelectIndex = -1;
			}
			SyncScreen();
			return FReply::Handled();
		}), 120.f) ]
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Cancel"), FOnClicked::CreateLambda([this]() { Screen = EMCScreen::Title; SyncScreen(); return FReply::Handled(); }), 120.f) ]
	];
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ];
}

TSharedRef<SWidget> SMCRootWidget::MakeCreateWorldScreen()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("World name"))).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ];
	Body->AddSlot().AutoHeight()
	[ SNew(SEditableTextBox).Text(FText::FromString(NewWorldName)).Font(MCUI::FontM())
		.OnTextChanged_Lambda([this](const FText& T) { NewWorldName = T.ToString(); }) ];
	Body->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 2.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("Seed (blank = random)"))).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ];
	Body->AddSlot().AutoHeight()
	[ SNew(SEditableTextBox).HintText(FText::FromString(TEXT("e.g. 12345"))).Font(MCUI::FontM())
		.OnTextChanged_Lambda([this](const FText& T) { NewWorldSeed = T.ToString(); }) ];
	Body->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 2.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("Game mode"))).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ];
	for (int32 i = 0; i < 4; ++i)
	{
		static const TCHAR* Modes[] = { TEXT("Survival"), TEXT("Creative"), TEXT("Adventure"), TEXT("Spectator") };
		Body->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f))
		[ SNew(SButton).ButtonColorAndOpacity(NewWorldMode == i ? MCUI::Accent : FLinearColor(0.3f, 0.3f, 0.33f)).ContentPadding(FMargin(10.f, 5.f))
			.OnClicked(FOnClicked::CreateLambda([this, i]() { NewWorldMode = i; SyncScreen(); return FReply::Handled(); }))
			[ SNew(STextBlock).Text(FText::FromString(Modes[i])).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(NewWorldMode == i ? FLinearColor::Black : MCUI::Text)) ] ];
	}
	Body->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
	[ SNew(SCheckBox).IsChecked_Lambda([this]() { return bNewWorldCheats ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bNewWorldCheats = S == ECheckBoxState::Checked; })
		[ SNew(STextBlock).Text(FText::FromString(TEXT("Allow cheats / commands"))).Font(MCUI::FontM()) ] ];

	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight()[ SNew(SBox).WidthOverride(380.f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(MCUI::Panel).Padding(FMargin(14.f))[ Body ] ] ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Create World"), FOnClicked::CreateLambda([this]()
		{
			AMCGame* G = Game();
			uint64 Seed = 0;
			if (!NewWorldSeed.IsEmpty()) Seed = FCString::Strtoui64(*NewWorldSeed, nullptr, 10);
			if (G) G->StartWorld(NewWorldName, Seed, false, (EMCGameMode)NewWorldMode);
			if (UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr)
			{
				GI->PendingWorld = NewWorldName;
				GI->PendingSeed = Seed;
				GI->bPendingLoad = false;
				GI->PendingMode = NewWorldMode;
			}
			Screen = EMCScreen::Playing;
			SyncScreen();
			return FReply::Handled();
		}), 200.f) ]
		+ SHorizontalBox::Slot().AutoWidth()[ ButtonText(TEXT("Cancel"), FOnClicked::CreateLambda([this]() { Screen = EMCScreen::Title; SyncScreen(); return FReply::Handled(); }), 150.f) ]
	];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f))
	[ SNew(STextBlock).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim))
		.Text(FText::FromString(TEXT("Worlds are stored in Saved/Opus55Worlds"))) ];
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ];
}

TSharedRef<SWidget> SMCRootWidget::MakeOptionsScreen()
{
	UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr;
	if (!GI) return SNew(SBox);
	FMCOptions& O = GI->Options;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[ Slider(TEXT("Mouse Sensitivity"), O.MouseSensitivity, 0.f, 1.f, [this, &O](float V) { O.MouseSensitivity = V; if (UMCGameInstance* G2 = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr) G2->SaveOptions(); }) ];
	Body->AddSlot().AutoHeight()[ Slider(TEXT("FOV"), O.FOV, 30.f, 110.f, [this, &O](float V) { O.FOV = V; if (AMCPlayer* P = Player()) P->FOVOverride = V; }) ];
	Body->AddSlot().AutoHeight()[ Slider(TEXT("Master Volume"), O.MasterVolume, 0.f, 1.f, [this, &O](float V) { O.MasterVolume = V; if (AMCGame* G2 = Game()) if (G2->Audio) G2->Audio->MasterVolume = V; }) ];
	Body->AddSlot().AutoHeight()[ Slider(TEXT("Music Volume"), O.MusicVolume, 0.f, 1.f, [this, &O](float V) { O.MusicVolume = V; if (AMCGame* G2 = Game()) if (G2->Audio) G2->Audio->MusicVolume = V; }) ];
	Body->AddSlot().AutoHeight()[ Slider(TEXT("Render Distance"), (float)O.RenderDistance, 2.f, 32.f, [this, &O](float V) { O.RenderDistance = FMath::RoundToInt(V); if (AMCGame* G2 = Game()) { G2->RenderDistance = O.RenderDistance; G2->ApplyOptions(); } }) ];
	Body->AddSlot().AutoHeight()[ Slider(TEXT("Brightness"), O.Brightness, 0.f, 1.f, [this, &O](float V) { O.Brightness = V; }) ];
	// GUI scale 0 = auto (largest that fits), applied as screens rebuild
	Body->AddSlot().AutoHeight()[ Slider(TEXT("GUI Scale (0 = Auto)"), (float)O.GuiScale, 0.f, 6.f, [this, &O](float V) { O.GuiScale = FMath::RoundToInt(V); }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("View Bobbing"), O.bViewBobbing, [this, &O](bool V) { O.bViewBobbing = V; }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Invert Mouse"), O.bInvertMouse, [this, &O](bool V) { O.bInvertMouse = V; }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Show FPS"), O.bShowFPS, [this, &O](bool V) { O.bShowFPS = V; }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Clouds"), O.bClouds, [this, &O](bool V) { O.bClouds = V; if (AMCGame* G2 = Game()) G2->ApplyOptions(); }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Shadows"), O.bShadows, [this, &O](bool V) { O.bShadows = V; if (AMCGame* G2 = Game()) G2->ApplyOptions(); }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Toggle Sprint"), O.bToggleSprint, [this, &O](bool V) { O.bToggleSprint = V; }) ];
	Body->AddSlot().AutoHeight()[ Toggle(TEXT("Toggle Sneak"), O.bToggleSneak, [this, &O](bool V) { O.bToggleSneak = V; }) ];

	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight()
	[ SNew(SBox).WidthOverride(420.f)
		[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(MCUI::Panel).Padding(FMargin(14.f))
			[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[ SNew(STextBlock).Text(FText::FromString(TEXT("Options"))).Font(MCUI::FontL()).ColorAndOpacity(FSlateColor(MCUI::Text)) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(SBox).HeightOverride(420.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ Body ] ] ]
			] ] ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f))
	[ ButtonText(TEXT("Done"), FOnClicked::CreateLambda([this]()
	{
		if (UMCGameInstance* G2 = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr) G2->SaveOptions();
		if (Game() && Game()->bTitleScreen) { Screen = EMCScreen::Title; bShowOptions = false; SyncScreen(); }
		else CloseOptions();
		return FReply::Handled();
	}), 200.f) ];
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ];
}

TSharedRef<SWidget> SMCRootWidget::MakePauseOverlay()
{
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 16.f))
	[ SNew(STextBlock).Text(FText::FromString(TEXT("Game Menu"))).Font(MCUI::FontL()).ColorAndOpacity(FSlateColor(MCUI::Text)).ShadowOffset(FVector2D(2.f, 2.f)) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Back to Game"), FOnClicked::CreateLambda([this]()
	{
		if (AMCPlayerController* C = Controller.Get()) { C->bPaused = false; if (AMCGame* G = Game()) G->bPaused = false; C->UpdateInputMode(); }
		SetPaused(false);
		return FReply::Handled();
	}), 340.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Options..."), FOnClicked::CreateLambda([this]() { OpenOptions(); return FReply::Handled(); }), 340.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Save World"), FOnClicked::CreateLambda([this]()
	{
		if (AMCGame* G = Game()) { G->SaveWorld(); G->AddChat(TEXT("§aWorld saved")); }
		return FReply::Handled();
	}), 340.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Save and Quit to Title"), FOnClicked::CreateLambda([this]()
	{
		if (AMCPlayerController* C = Controller.Get()) { C->bPaused = false; }
		if (AMCGame* G = Game()) { G->bPaused = false; G->QuitToTitle(); }
		bPaused = false;
		Screen = EMCScreen::Title;
		SyncScreen();
		if (AMCPlayerController* C = Controller.Get()) C->UpdateInputMode();
		return FReply::Handled();
	}), 340.f) ];
	return SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ];
}

TSharedRef<SWidget> SMCRootWidget::MakeDeathScreen()
{
	AMCPlayer* P = Player();
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().FillHeight(0.3f)[ SNullWidget::NullWidget ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[ SNew(STextBlock).Text(FText::FromString(TEXT("You Died!"))).Font(MCUI::FontXL()).ColorAndOpacity(FSlateColor(MCUI::Danger)).ShadowOffset(FVector2D(3.f, 3.f)) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f))
	[ SNew(STextBlock).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(MCUI::Text))
		.Text(FText::FromString(P && !P->LastDeathMessage.IsEmpty() ? P->LastDeathMessage : TEXT("Better luck next time"))) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f))
	[ SNew(STextBlock).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim))
		.Text(FText::FromString(FString::Printf(TEXT("Score: %d   Deaths: %lld"), P ? P->Score : 0, P ? P->StatDeaths : 0))) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 14.f))
	[ ButtonText(TEXT("Respawn"), FOnClicked::CreateLambda([this]()
	{
		AMCPlayer* PP = Player();
		const bool bHardcore = Game() && Game()->bHardcore;
		if (PP) { PP->Respawn(); }
		if (bHardcore && Game()) { Game()->AddChat(TEXT("Game over! Hardcore world ended.")); Game()->QuitToTitle(); Screen = EMCScreen::Title; }
		(void)bHardcore;
		SyncScreen();
		return FReply::Handled();
	}), 300.f) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center)[ ButtonText(TEXT("Title Screen"), FOnClicked::CreateLambda([this]()
	{
		if (AMCGame* G = Game()) G->QuitToTitle();
		Screen = EMCScreen::Title;
		SyncScreen();
		return FReply::Handled();
	}), 300.f) ];
	Col->AddSlot().FillHeight(0.7f)[ SNullWidget::NullWidget ];
	return Col;
}

TSharedRef<SWidget> SMCRootWidget::MakeCreditsScreen()
{
	static const TCHAR* Lines[] = {
		TEXT("Opus 5.5 Minecraft"),
		TEXT("An original recreation of the voxel sandbox genre"),
		TEXT(""),
		TEXT("Engine: Unreal Engine 5"),
		TEXT("World generation, blocks, mobs, textures and sounds"),
		TEXT("are all generated procedurally by this project."),
		TEXT(""),
		TEXT("No Mojang code, textures, sounds or fonts were used."),
		TEXT(""),
		TEXT("Thank you for playing.")
	};
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	for (const TCHAR* L : Lines)
		Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f))
		[ SNew(STextBlock).Text(FText::FromString(L)).Font(MCUI::FontL()).ColorAndOpacity(FSlateColor(MCUI::Text)) ];
	Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 24.f))
	[ ButtonText(TEXT("Done"), FOnClicked::CreateLambda([this]()
	{
		if (AMCGame* G = Game()) G->bShowCredits = false;
		Screen = EMCScreen::Playing;
		SyncScreen();
		return FReply::Handled();
	}), 240.f) ];
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ];
}

TSharedRef<SWidget> SMCRootWidget::MakeLoadingScreen()
{
	AMCGame* G = Game();
	const FString Text = G && !G->LoadingText.IsEmpty() ? G->LoadingText : TEXT("Building terrain");
	const float Progress = G ? FMath::Clamp(G->LoadingProgress, 0.f, 1.f) : 0.f;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[ SNew(STextBlock).Text(FText::FromString(Text)).Font(MCUI::FontXL()).ColorAndOpacity(FSlateColor(MCUI::Text)) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 12.f))
		[ SNew(SBox).WidthOverride(320.f).HeightOverride(8.f)
			[ SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f)) ]
				+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride(320.f * Progress)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(MCUI::Accent) ] ]
			] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
		[ SNew(STextBlock).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim))
			.Text(FText::FromString(G ? FString::Printf(TEXT("%s - seed %llu"), *G->WorldName, G->Seed) : FString())) ]
	];
}
