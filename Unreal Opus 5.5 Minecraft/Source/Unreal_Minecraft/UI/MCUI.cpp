#include "UI/MCUI.h"
#include "Render/MCIcons.h"
#include "Items/MCItems.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

namespace MCUI
{
	const FLinearColor Panel(0.09f, 0.09f, 0.11f, 0.94f);
	const FLinearColor PanelLight(0.16f, 0.16f, 0.18f, 0.95f);
	const FLinearColor SlotBg(0.20f, 0.20f, 0.22f, 0.95f);
	const FLinearColor SlotEdge(0.06f, 0.06f, 0.07f, 1.f);
	const FLinearColor Text(0.95f, 0.95f, 0.95f, 1.f);
	const FLinearColor TextDim(0.70f, 0.70f, 0.73f, 1.f);
	const FLinearColor Accent(0.55f, 0.85f, 0.35f, 1.f);
	const FLinearColor Danger(0.90f, 0.28f, 0.22f, 1.f);
	const FLinearColor Gold(0.98f, 0.82f, 0.28f, 1.f);

	// fonts are created lazily: FCoreStyle's default font needs the engine content paths, which are not ready at static-init time
	const FSlateFontInfo& FontS() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", 11); return F; }
	const FSlateFontInfo& FontM() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", 14); return F; }
	const FSlateFontInfo& FontL() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", 20); return F; }
	const FSlateFontInfo& FontXL() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", 38); return F; }
	const FSlateFontInfo& FontMono() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Mono", 12); return F; }
	const FSlateFontInfo& FontPanel() { static FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", 6); return F; }
	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("GenericWhiteBox"); }

	void PaintStack(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& LocalPos, float Size, const FMCItemStack& Stack)
	{
		if (Stack.IsEmpty() || Stack.Id == 0) return;
		const float Inner = Size * 0.84f;
		const FVector2D IconPos = LocalPos + FVector2D((Size - Inner) * 0.5f, (Size - Inner) * 0.5f);
		if (const FSlateBrush* Icon = MCIcons::GetBrush(Stack.Id))
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(Inner, Inner), FSlateLayoutTransform(IconPos)), Icon, ESlateDrawEffect::None, FLinearColor::White);
			if (MCIcons::HasGlint(Stack))
			{
				if (const FSlateBrush* Glint = MCIcons::GetGlyph(TEXT("glint")))
				{
					const float T = FMath::Fmod((float)FPlatformTime::Seconds() * 0.6f, 1.f);
					FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(Inner, Inner), FSlateLayoutTransform(IconPos)), Glint,
						ESlateDrawEffect::None, FLinearColor(0.7f, 0.55f + 0.3f * T, 1.f, 0.45f));
				}
			}
		}
		// durability
		if (Stack.IsDamageable() && Stack.Damage > 0)
		{
			const int32 MaxDur = FMath::Max(1, Stack.Item().MaxDamage);
			const float Frac = 1.f - FMath::Clamp(Stack.Damage / (float)MaxDur, 0.f, 1.f);
			const float BarW = Size * 0.8f, BarH = FMath::Max(2.f, Size * 0.07f);
			const FVector2D BarPos = LocalPos + FVector2D(Size * 0.1f, Size * 0.84f);
			FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(BarW, BarH), FSlateLayoutTransform(BarPos)), White(), ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.9f));
			const FLinearColor C = FLinearColor::LerpUsingHSV(FLinearColor(0.9f, 0.15f, 0.1f), FLinearColor(0.2f, 0.9f, 0.2f), Frac);
			FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(BarW * Frac, BarH * 0.6f), FSlateLayoutTransform(BarPos)), White(), ESlateDrawEffect::None, C);
		}
		// count
		if (Stack.Count > 1)
		{
			const FString Count = FString::FromInt(Stack.Count);
			// sized with the slot (menus draw at GUI-pixel scale and are magnified by the DPI scaler afterwards)
			const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(1, FMath::RoundToInt(Size * 0.4f)));
			const float Tw = Count.Len() * Size * 0.2f;
			const FVector2D TP = LocalPos + FVector2D(Size - Tw - Size * 0.06f, Size * 0.52f);
			FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(FVector2D(Size, Size * 0.5f), FSlateLayoutTransform(TP + FVector2D(1.f, 1.f))), Count, Font, ESlateDrawEffect::None, FLinearColor(0.15f, 0.15f, 0.15f, 1.f));
			FSlateDrawElement::MakeText(Out, Layer + 5, Geo.ToPaintGeometry(FVector2D(Size, Size * 0.5f), FSlateLayoutTransform(TP)), Count, Font, ESlateDrawEffect::None, FLinearColor::White);
		}
	}
}

void SMCItemStackWidget::Construct(const FArguments& InArgs)
{
	Size = InArgs._Size;
	Getter = InArgs._Getter;
	SetCanTick(false);
}

int32 SMCItemStackWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!Getter) return LayerId;
	const FMCItemStack S = Getter();
	const FVector2D Local = AllottedGeometry.GetLocalSize();
	MCUI::PaintStack(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, FMath::Min(Local.X, Local.Y), S);
	return LayerId + 7;
}

static int32 SlotButtonIndex(const FPointerEvent& Ev)
{
	const FKey B = Ev.GetEffectingButton();
	return B == EKeys::RightMouseButton ? 1 : (B == EKeys::MiddleMouseButton ? 2 : 0);
}

FReply SMCSlotHit::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (OnClick) OnClick(SlotButtonIndex(Ev));
	return FReply::Handled();
}

FReply SMCSlotHit::OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& Ev)
{
	// Slate turns the second press into a double-click event: forward it as a normal press, the menu detects the pair
	if (OnClick) OnClick(SlotButtonIndex(Ev));
	return FReply::Handled();
}

void SMCSlotHit::OnMouseEnter(const FGeometry& Geo, const FPointerEvent& Ev)
{
	SLeafWidget::OnMouseEnter(Geo, Ev);
	if (OnHover) OnHover(true);
}

void SMCSlotHit::OnMouseLeave(const FPointerEvent& Ev)
{
	SLeafWidget::OnMouseLeave(Ev);
	if (OnHover) OnHover(false);
}

int32 SMCSlotHit::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (IsHovered())
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 8, AllottedGeometry.ToPaintGeometry(), MCUI::White(), ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.28f));
	return LayerId + 8;
}

int32 SMCCursorStack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!FSlateApplication::IsInitialized()) return LayerId;
	const FVector2D Mouse = AllottedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	if (Getter)
	{
		const FMCItemStack S = Getter();
		if (!S.IsEmpty()) MCUI::PaintStack(OutDrawElements, LayerId + 10, AllottedGeometry, Mouse - FVector2D(Size * 0.5f, Size * 0.5f), Size, S);
	}
	if (Tooltip)
	{
		const FString T = Tooltip();
		if (!T.IsEmpty())
		{
			const FSlateFontInfo& Font = MCUI::FontM();
			const float W = FMath::Max(80.f, T.Len() * 8.5f + 16.f);
			const FVector2D P = Mouse + FVector2D(16.f, -26.f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 20, AllottedGeometry.ToPaintGeometry(FVector2D(W, 26.f), FSlateLayoutTransform(P)), MCUI::White(), ESlateDrawEffect::None, FLinearColor(0.07f, 0.02f, 0.1f, 0.94f));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 21, AllottedGeometry.ToPaintGeometry(FVector2D(W, 2.f), FSlateLayoutTransform(P)), MCUI::White(), ESlateDrawEffect::None, FLinearColor(0.35f, 0.1f, 0.7f, 1.f));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 22, AllottedGeometry.ToPaintGeometry(FVector2D(W, 24.f), FSlateLayoutTransform(P + FVector2D(8.f, 4.f))), T, Font, ESlateDrawEffect::None, MCUI::Text);
		}
	}
	return LayerId + 23;
}
