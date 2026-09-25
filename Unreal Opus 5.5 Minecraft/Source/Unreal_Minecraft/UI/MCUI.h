// Shared Slate style constants and live item widgets for the Minecraft-style UI.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Items/MCItems.h"

namespace MCUI
{
	extern const FLinearColor Panel;
	extern const FLinearColor PanelLight;
	extern const FLinearColor SlotBg;
	extern const FLinearColor SlotEdge;
	extern const FLinearColor Text;
	extern const FLinearColor TextDim;
	extern const FLinearColor Accent;
	extern const FLinearColor Danger;
	extern const FLinearColor Gold;
	const FSlateFontInfo& FontS();
	const FSlateFontInfo& FontM();
	const FSlateFontInfo& FontL();
	const FSlateFontInfo& FontXL();
	const FSlateFontInfo& FontMono();
	/** Small font for text drawn inside GUI-scaled panels (8 px design size). */
	const FSlateFontInfo& FontPanel();
	const FSlateBrush* White();

	/** Paints an item stack (icon, glint, count, durability bar) into a rectangle. */
	void PaintStack(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& LocalPos, float Size, const FMCItemStack& Stack);
}

/** Leaf widget that paints whatever stack its getter returns (reads live data every frame). */
class SMCItemStackWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMCItemStackWidget) : _Size(36.f) {}
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(TFunction<FMCItemStack()>, Getter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Size = 36.f;
	TFunction<FMCItemStack()> Getter;
};

/** Invisible slot hit area: reports mouse-downs (0 = left, 1 = right, 2 = middle) and paints a hover highlight. */
class SMCSlotHit : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMCSlotHit) {}
		SLATE_ARGUMENT(TFunction<void(int32 /*Button*/)>, OnClick)
		SLATE_ARGUMENT(TFunction<void(bool /*bEnter*/)>, OnHover)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { OnClick = InArgs._OnClick; OnHover = InArgs._OnHover; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.f, 16.f); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual void OnMouseEnter(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual void OnMouseLeave(const FPointerEvent& Ev) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TFunction<void(int32)> OnClick;
	TFunction<void(bool)> OnHover;
};

/** Full-screen, hit-test invisible layer that draws the carried stack under the mouse cursor. */
class SMCCursorStack : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMCCursorStack) : _Size(40.f) {}
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(TFunction<FMCItemStack()>, Getter)
		SLATE_ARGUMENT(TFunction<FString()>, Tooltip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Size = InArgs._Size; Getter = InArgs._Getter; Tooltip = InArgs._Tooltip; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Size = 40.f;
	TFunction<FMCItemStack()> Getter;
	TFunction<FString()> Tooltip;
};
