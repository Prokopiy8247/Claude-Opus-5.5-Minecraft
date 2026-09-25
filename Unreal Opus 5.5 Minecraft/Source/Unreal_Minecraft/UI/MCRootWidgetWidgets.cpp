// Slate UI: shared building blocks (themed panels, buttons, item icons, slot widgets, chat text entry).
#include "Engine/UserInterfaceSettings.h"
#include "UI/MCRootWidget.h"
#include "UI/MCUI.h"
#include "Render/MCIcons.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCGameMode.h"
#include "Game/MCMob.h"
#include "World/MCBlockEntity.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/MCMenu.h"
#include "Audio/MCAudio.h"
#include "Widgets/SCanvas.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Fonts/SlateFontInfo.h"

AMCGame* SMCRootWidget::Game() const { return Controller.IsValid() ? Controller->GetGame() : AMCGame::Get(Controller.Get()); }
AMCPlayer* SMCRootWidget::Player() const { AMCGame* G = Game(); return G ? G->Player.Get() : nullptr; }
FMCMenu* SMCRootWidget::Menu() const { AMCPlayer* P = Player(); return P ? P->ActiveMenu() : nullptr; }


TSharedRef<SWidget> SMCRootWidget::Icon(int32 ItemId, float Size)
{
	const FSlateBrush* B = MCIcons::GetBrush((FMCItemId)FMath::Max(0, ItemId));
	if (!B) return SNew(SBox).WidthOverride(Size).HeightOverride(Size);
	return SNew(SBox)
		.WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SImage)
			.Image(B)
			.DesiredSizeOverride(FVector2D(Size, Size))
		];
}

TSharedRef<SWidget> SMCRootWidget::Glyph(const TCHAR* Name, float Size)
{
	const FSlateBrush* B = MCIcons::GetGlyph(FName(Name));
	if (!B) return SNew(SBox).WidthOverride(Size).HeightOverride(Size);
	return SNew(SBox)
		.WidthOverride(Size).HeightOverride(Size)
		[ SNew(SImage).Image(B).DesiredSizeOverride(FVector2D(Size, Size)) ];
}

TSharedRef<SWidget> SMCRootWidget::ButtonText(const FString& Label, const FOnClicked& OnClick, float Width, bool bDisabled)
{
	return SNew(SBox)
		.WidthOverride(Width)
		.Padding(FMargin(2.f))
		[
			SNew(SButton)
			.ButtonColorAndOpacity(bDisabled ? FLinearColor(0.25f, 0.25f, 0.27f) : FLinearColor(0.35f, 0.35f, 0.38f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(8.f, 6.f))
			.IsEnabled(!bDisabled)
			.OnClicked(OnClick)
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(MCUI::Text))
			]
		];
}

TSharedRef<SWidget> SMCRootWidget::Toggle(const FString& Label, bool bValue, TFunction<void(bool)> OnChange)
{
	return SNew(SBox).Padding(FMargin(0.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(FText::FromString(Label)).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(bValue ? MCUI::Accent : FLinearColor(0.3f, 0.3f, 0.32f))
				.ContentPadding(FMargin(12.f, 4.f))
				.OnClicked(FOnClicked::CreateLambda([OnChange, bValue]() { OnChange(!bValue); return FReply::Handled(); }))
				[ SNew(STextBlock).Text(FText::FromString(bValue ? TEXT("ON") : TEXT("OFF"))).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(FLinearColor::Black)) ]
			]
		];
}

TSharedRef<SWidget> SMCRootWidget::Slider(const FString& Label, float Value, float Min, float Max, TFunction<void(float)> OnChange)
{
	TSharedRef<STextBlock> ValueText = SNew(STextBlock).Font(MCUI::FontS()).ColorAndOpacity(FSlateColor(MCUI::TextDim));
	const bool bPercent = Max <= 1.01f;
	ValueText->SetText(FText::FromString(bPercent ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f)) : FString::Printf(TEXT("%.1f"), Value)));
	return SNew(SBox).Padding(FMargin(0.f, 3.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(FText::FromString(Label)).Font(MCUI::FontM()).ColorAndOpacity(FSlateColor(MCUI::TextDim)) ]
				+ SHorizontalBox::Slot().AutoWidth()[ ValueText ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
			[
				SNew(SSlider)
				.Value((Value - Min) / FMath::Max(0.0001f, Max - Min))
				.OnValueChanged(FOnFloatValueChanged::CreateLambda([OnChange, Min, Max, ValueText, bPercent](float V)
				{
					const float Abs = Min + V * (Max - Min);
					ValueText->SetText(FText::FromString(bPercent ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Abs * 100.f)) : FString::Printf(TEXT("%.1f"), Abs)));
					OnChange(Abs);
				}))
			]
		];
}

TSharedRef<SWidget> SMCRootWidget::ScreenPanel(const FString& Title, TSharedRef<SWidget> Body, float Width, float Height)
{
	return SNew(SBox).WidthOverride(Width).Padding(FMargin(0.f, 0.f, 0.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(MCUI::Panel)
			.Padding(FMargin(12.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[ SNew(STextBlock).Text(FText::FromString(Title)).Font(MCUI::FontL()).ColorAndOpacity(FSlateColor(MCUI::Text)) ]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[ SNew(SBox).HeightOverride(Height > 0.f ? Height : 0.f)[ Body ] ]
			]
		];
}

// ---------------------------------------------------------------------------------------------------------------------
// GUI scale

float SMCRootWidget::GuiScaleFactor() const
{
	// Minecraft GUI scale: an integer number of screen pixels per GUI pixel; "auto" is the largest scale that still
	// fits a 320x240 GUI. The result is in Slate units, so the engine's resolution DPI curve is divided back out.
	float W = 1920.f, H = 1080.f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0) { W = Size.X; H = Size.Y; }
	}
	const int32 MaxScale = FMath::Max(1, FMath::FloorToInt(FMath::Min(W / 320.f, H / 240.f)));
	const UMCGameInstance* GI = Controller.IsValid() ? Cast<UMCGameInstance>(Controller->GetGameInstance()) : nullptr;
	const int32 Wanted = GI ? GI->Options.GuiScale : 0;
	const int32 Pixels = Wanted <= 0 ? MaxScale : FMath::Clamp(Wanted, 1, MaxScale);
	const float DPI = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint((int32)W, (int32)H));
	return (float)Pixels / FMath::Max(DPI, 0.1f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Slots

FMCItemStack SMCRootWidget::StackInSlot(int32 SlotIndex) const
{
	// menu slot index while a menu is open, otherwise a raw player inventory index (HUD hotbar)
	if (FMCMenu* M = Menu()) return M->Slots.IsValidIndex(SlotIndex) ? M->Slots[SlotIndex].Get() : FMCItemStack();
	AMCPlayer* P = Player();
	return (P && P->Inventory.Slots.IsValidIndex(SlotIndex)) ? P->Inventory.Slots[SlotIndex] : FMCItemStack();
}

TSharedRef<SWidget> SMCRootWidget::Slot(int32 SlotIndex, float Size)
{
	const FSlateBrush* Empty = nullptr;
	if (FMCMenu* M = Menu())
		if (M->Slots.IsValidIndex(SlotIndex) && !M->Slots[SlotIndex].EmptyIcon.IsNone()) Empty = MCIcons::GetGlyph(M->Slots[SlotIndex].EmptyIcon);

	TSharedRef<SOverlay> Cell = SNew(SOverlay);
	Cell->AddSlot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(MCUI::SlotBg) ];
	Cell->AddSlot().Padding(FMargin(1.f))[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.13f, 0.13f, 0.14f, 0.95f)) ];
	if (Empty)
		Cell->AddSlot().Padding(FMargin(Size * 0.1f))
		[
			SNew(SImage).Image(Empty)
			.ColorAndOpacity_Lambda([this, SlotIndex]() { return FSlateColor(StackInSlot(SlotIndex).IsEmpty() ? FLinearColor(1.f, 1.f, 1.f, 0.45f) : FLinearColor(0.f, 0.f, 0.f, 0.f)); })
		];
	// live contents: the painter reads the slot every frame, so screens never need rebuilding for item moves
	Cell->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
	[ SNew(SMCItemStackWidget).Size(Size - 2.f).Getter([this, SlotIndex]() { return StackInSlot(SlotIndex); }).Visibility(EVisibility::HitTestInvisible) ];
	Cell->AddSlot()
	[
		SNew(SMCSlotHit)
		.OnClick([this, SlotIndex](int32 Button) { OnSlotClick(SlotIndex, Button); })
		.OnHover([this, SlotIndex](bool bEnter) { if (bEnter) SlotFocus = SlotIndex; else if (SlotFocus == SlotIndex) SlotFocus = -1; })
	];
	return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[ Cell ];
}

TSharedRef<SWidget> SMCRootWidget::CreativeCell(int32 ItemId, float Size)
{
	const FMCItemStack Shown(ItemId, 1);
	return SNew(SBox)
		.WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(MCUI::SlotBg) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[ SNew(SMCItemStackWidget).Size(Size - 2.f).Getter([Shown]() { return Shown; }).Visibility(EVisibility::HitTestInvisible) ]
			+ SOverlay::Slot()
			[
				SNew(SMCSlotHit)
				.OnClick([this, ItemId](int32 Button) { OnCreativeClick(ItemId, Button); })
				.OnHover([this, ItemId](bool bEnter) { HoverItem = bEnter ? ItemId : (HoverItem == ItemId ? 0 : HoverItem); })
			]
		];
}

void SMCRootWidget::OnCreativeClick(int32 ItemId, int32 Button)
{
	FMCMenu* M = Menu();
	if (!M) return;
	FMCItemStack& Carried = M->Carried();
	const bool bShift = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	const FMCItemStack Pick(ItemId, 1);
	if (!Carried.IsEmpty())
	{
		// clicking the palette with a stack in hand: same item adds one (left) / deletes (right); others are destroyed
		if (Carried.Id == ItemId && Button == 0 && Carried.Count < Carried.MaxStack()) { ++Carried.Count; return; }
		Carried = FMCItemStack();
		return;
	}
	if (bShift)
	{
		// shift-click sends a full stack straight to the hotbar/inventory
		if (AMCPlayer* P = Player()) { FMCItemStack Full = Pick; Full.Count = Pick.MaxStack(); P->AddItem(Full); }
		return;
	}
	Carried = Pick;
	if (Button == 2 || Button == 1) Carried.Count = Pick.MaxStack();
}

FReply SMCRootWidget::OnSlotClick(int32 SlotIndex, int32 Button)
{
	AMCPlayer* P = Player();
	if (!P) return FReply::Handled();
	FMCMenu* M = Menu();
	if (!M)
	{
		// HUD hotbar: clicking selects the slot
		if (SlotIndex >= 0 && SlotIndex < 9) P->SelectSlot(SlotIndex);
		return FReply::Handled();
	}
	const double Now = FPlatformTime::Seconds();
	const bool bShift = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	const bool bDouble = Button == 0 && SlotIndex == LastClickSlot && Now - LastClickTime < 0.3;
	LastClickTime = Now;
	LastClickSlot = SlotIndex;
	EMCClick Mode = EMCClick::Pick;
	if (SlotIndex == -999) Mode = EMCClick::Pick;                   // outside the panel: drop
	else if (Button == 2) Mode = EMCClick::Clone;                  // middle click (creative copy)
	else if (bShift) Mode = EMCClick::QuickMove;
	else if (bDouble && !M->Carried().IsEmpty()) Mode = EMCClick::PickAll;
	M->Click(SlotIndex, Button == 2 ? 0 : Button, Mode);
	if (AMCGame* G = Game()) if (G->Audio && Mode != EMCClick::Pick) G->Audio->Play2D(TEXT("ui.click"), 0.25f);
	return FReply::Handled();
}

bool SMCRootWidget::HotbarKeyOverSlot(int32 HotbarKey)
{
	// 1-9 over a hovered slot swaps it with that hotbar slot (F = offhand, key 40)
	FMCMenu* M = Menu();
	if (!M || !M->Slots.IsValidIndex(SlotFocus)) return false;
	M->Click(SlotFocus, 0, EMCClick::Swap, HotbarKey);
	return true;
}

bool SMCRootWidget::ThrowOverSlot(bool bWholeStack)
{
	FMCMenu* M = Menu();
	if (!M || !M->Slots.IsValidIndex(SlotFocus)) return false;
	M->Click(SlotFocus, bWholeStack ? 1 : 0, EMCClick::Throw);
	return true;
}

FString SMCRootWidget::HoverTooltip() const
{
	FMCMenu* M = Menu();
	if (M && !M->Carried().IsEmpty()) return FString();
	FMCItemStack S;
	if (SlotFocus >= 0) S = StackInSlot(SlotFocus);
	else if (HoverItem > 0) S = FMCItemStack(HoverItem, 1);
	if (S.IsEmpty()) return FString();
	FString T = S.GetDisplayName();
	if (S.IsDamageable() && S.Damage > 0) T += FString::Printf(TEXT("  (%d/%d)"), S.Item().MaxDamage - S.Damage, S.Item().MaxDamage);
	return T;
}

// ---------------------------------------------------------------------------------------------------------------------
// Menu panels (inventory, container, creative): backdrop + slots + station widgets + carried stack

TSharedRef<SWidget> SMCRootWidget::WrapMenu(TSharedRef<SWidget> Content)
{
	const float G = GuiScaleFactor();
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f)) ]
		// clicking the dimmed backdrop drops the carried stack (vanilla: click outside the window)
		+ SOverlay::Slot()
		[ SNew(SMCSlotHit).OnClick([this](int32 Button) { if (Button < 2) OnSlotClick(-999, Button); }) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[ SNew(SDPIScaler).DPIScale(G)[ Content ] ]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SMCCursorStack)
			.Size(18.f * G)
			.Getter([this]() { FMCMenu* M = Menu(); return M ? M->Carried() : FMCItemStack(); })
			.Tooltip([this]() { return HoverTooltip(); })
			.Visibility(EVisibility::HitTestInvisible)
		];
}

TSharedRef<SWidget> SMCRootWidget::MakeMenuPanel(FMCMenu* M)
{
	const float Step = 18.f;
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	// panel backdrop at the size of this menu (176x166 for most, wider for chests / creative)
	Canvas->AddSlot().Position(FVector2D(0.f, 0.f)).Size(M->PanelSize)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FLinearColor(0.78f, 0.78f, 0.80f, 1.f))
		.Padding(FMargin(1.f))
		[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.42f, 0.42f, 0.45f, 0.98f)) ]
	];
	// titles: container name top-left, "Inventory" above the player grid
	// the player inventory titles its 2x2 crafting grid (Minecraft: x 97), other menus sit top-left
	const FVector2D TitlePos = M->Type == EMCMenuType::Inventory ? FVector2D(97.f, 4.f) : FVector2D(8.f, 4.f);
	Canvas->AddSlot().Position(TitlePos).Size(FVector2D(M->PanelSize.X - TitlePos.X - 8.f, 10.f))
	[ SNew(STextBlock).Text(FText::FromString(M->Title)).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.12f, 0.13f, 1.f))) ];
	if (M->Type != EMCMenuType::Inventory)
		Canvas->AddSlot().Position(FVector2D(8.f, M->PlayerInvPos.Y - 11.f)).Size(FVector2D(120.f, 10.f))
		[ SNew(STextBlock).Text(FText::FromString(TEXT("Inventory"))).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.12f, 0.13f, 1.f))) ];
	for (int32 i = 0; i < M->Slots.Num(); ++i)
	{
		const FMCSlot& S = M->Slots[i];
		Canvas->AddSlot().Position(S.UIPos - FVector2D(1.f, 1.f)).Size(FVector2D(Step, Step))[ Slot(i, Step) ];
	}

	// station widgets: live progress bars, enchant offers, recipe grids, beacon powers
	auto Bar = [this](int32 Which, const FLinearColor& C, float W, float H)
	{
		return SNew(SBox).WidthOverride(W).HeightOverride(H)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.22f, 0.9f)) ]
			+ SOverlay::Slot().HAlign(HAlign_Left)
			[
				SNew(SBox).WidthOverride_Lambda([this, Which, W]() { FMCMenu* MM = Menu(); return FOptionalSize(MM ? FMath::Clamp(MM->GetProgress(Which), 0.f, 1.f) * W : 0.f); })
				[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(C) ]
			]
		];
	};
	switch (M->Type)
	{
	case EMCMenuType::Furnace:
	case EMCMenuType::BlastFurnace:
	case EMCMenuType::Smoker:
		// progress arrow between input and output, flame (fuel) under the input
		Canvas->AddSlot().Position(FVector2D(79.f, 34.f)).Size(FVector2D(24.f, 17.f))[ Bar(0, FLinearColor(0.95f, 0.95f, 0.95f, 1.f), 24.f, 6.f) ];
		Canvas->AddSlot().Position(FVector2D(56.f, 36.f)).Size(FVector2D(14.f, 14.f))[ Bar(1, FLinearColor(1.f, 0.55f, 0.1f, 1.f), 14.f, 12.f) ];
		break;
	case EMCMenuType::Brewing:
		Canvas->AddSlot().Position(FVector2D(97.f, 16.f)).Size(FVector2D(9.f, 28.f))[ Bar(0, FLinearColor(0.95f, 0.95f, 0.95f, 1.f), 9.f, 28.f) ];
		Canvas->AddSlot().Position(FVector2D(60.f, 44.f)).Size(FVector2D(18.f, 4.f))[ Bar(1, MCUI::Gold, 18.f, 4.f) ];
		break;
	case EMCMenuType::Enchanting:
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 Cost = M->GetData(i);
			const FString Text = M->GetText(i);
			const bool bCan = Cost > 0 && Player() && (Player()->IsCreative() || Player()->XPLevel >= Cost);
			Canvas->AddSlot().Position(FVector2D(60.f, 14.f + i * 19.f)).Size(FVector2D(108.f, 19.f))
			[
				SNew(SButton)
				.ButtonColorAndOpacity(bCan ? FLinearColor(0.55f, 0.45f, 0.65f) : FLinearColor(0.3f, 0.3f, 0.32f))
				.ContentPadding(FMargin(3.f, 1.f))
				.OnClicked(FOnClicked::CreateLambda([this, i]() { if (FMCMenu* MM = Menu()) MM->ButtonClick(i); SyncScreen(); return FReply::Handled(); }))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[ SNew(STextBlock).Text(FText::FromString(Text.IsEmpty() ? TEXT("") : Text)).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(bCan ? MCUI::Text : MCUI::TextDim)) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(STextBlock).Text(FText::FromString(Cost > 0 ? FString::FromInt(Cost) : FString())).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(MCUI::Accent)) ]
				]
			];
		}
		break;
	}
	case EMCMenuType::Stonecutter:
	{
		const int32 Count = M->GetData(0);
		const int32 Selected = M->GetData(1);
		for (int32 i = 0; i < FMath::Min(Count, 12); ++i)
		{
			const int32 ItemId = M->GetData(2 + i);
			const FMCItemStack Shown(ItemId, 1);
			Canvas->AddSlot().Position(FVector2D(52.f + (i % 4) * 16.f, 14.f + (i / 4) * 18.f)).Size(FVector2D(16.f, 18.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("GenericWhiteBox")).ColorAndOpacity(i == Selected ? MCUI::Accent : MCUI::SlotBg) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[ SNew(SMCItemStackWidget).Size(15.f).Getter([Shown]() { return Shown; }).Visibility(EVisibility::HitTestInvisible) ]
				+ SOverlay::Slot()[ SNew(SMCSlotHit).OnClick([this, i](int32) { if (FMCMenu* MM = Menu()) MM->ButtonClick(i); }) ]
			];
		}
		break;
	}
	case EMCMenuType::Beacon:
	{
		static const TCHAR* Names[] = { TEXT("Speed"), TEXT("Haste"), TEXT("Resistance"), TEXT("Jump Boost"), TEXT("Strength") };
		for (int32 i = 0; i < 5; ++i)
			Canvas->AddSlot().Position(FVector2D(20.f + (i % 3) * 52.f, 18.f + (i / 3) * 22.f)).Size(FVector2D(50.f, 20.f))
			[
				SNew(SButton).ContentPadding(FMargin(2.f, 1.f))
				.OnClicked(FOnClicked::CreateLambda([this, i]() { if (FMCMenu* MM = Menu()) MM->ButtonClick(i + 1); SyncScreen(); return FReply::Handled(); }))
				[ SNew(STextBlock).Text(FText::FromString(Names[i])).Font(MCUI::FontPanel()) ]
			];
		Canvas->AddSlot().Position(FVector2D(20.f, 64.f)).Size(FVector2D(140.f, 10.f))
		[ SNew(STextBlock).Font(MCUI::FontPanel()).ColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.12f, 0.13f, 1.f)))
			.Text(FText::FromString(FString::Printf(TEXT("Pyramid level %d"), M->GetData(0)))) ];
		break;
	}
	default: break;
	}

	return SNew(SBox).WidthOverride(M->PanelSize.X).HeightOverride(M->PanelSize.Y)[ Canvas ];
}
