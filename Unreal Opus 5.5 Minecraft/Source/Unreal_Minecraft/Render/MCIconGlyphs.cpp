// Item icon painter, part 3: UI glyphs (hearts, hunger, armour, bubbles, icons) drawn into the same canvas.
#include "Render/MCIcons.h"
#include "Render/MCIconCanvas.h"

using namespace MCIconDraw;

namespace
{
	const FLinearColor Red = Hex(0xC81414);
	const FLinearColor RedLight = Hex(0xF04040);
	const FLinearColor DarkRed = Hex(0x6A0A0A);
	const FLinearColor White = Hex(0xF4F4F4);
	const FLinearColor Grey = Hex(0x8A8A8A);
	const FLinearColor Dark = Hex(0x1E1E1E);
	const FLinearColor Gold = Hex(0xE8C23A);
	const FLinearColor Green = Hex(0x4AC83A);

	void HeartShape(FIco& C, const FLinearColor& Outer, const FLinearColor& Inner)
	{
		C.Poly({ { 8.f, 14.5f }, { 1.5f, 8.f }, { 1.5f, 4.5f }, { 4.5f, 2.f }, { 8.f, 4.5f }, { 11.5f, 2.f }, { 14.5f, 4.5f }, { 14.5f, 8.f } }, Outer);
		C.Disc(4.6f, 4.6f, 2.4f, Outer);
		C.Disc(11.4f, 4.6f, 2.4f, Outer);
		if (Inner.A > 0.f)
		{
			C.Poly({ { 8.f, 12.5f }, { 3.5f, 7.f }, { 3.5f, 5.f }, { 5.5f, 3.5f }, { 8.f, 5.8f }, { 10.5f, 3.5f }, { 12.5f, 5.f }, { 12.5f, 7.f } }, Inner);
			C.Disc(5.6f, 5.6f, 1.6f, Inner);
			C.Disc(10.4f, 5.6f, 1.6f, Inner);
		}
	}

	void HungerShape(FIco& C, const FLinearColor& C1, const FLinearColor& C2)
	{
		C.Poly({ { 3.f, 13.f }, { 3.f, 6.f }, { 6.f, 3.f }, { 11.f, 3.5f }, { 13.f, 7.f }, { 12.f, 11.f }, { 9.f, 13.5f } }, C1);
		C.Disc(6.5f, 7.f, 3.f, C1);
		C.Disc(6.5f, 7.f, 1.6f, C2);
		C.Line(3.f, 6.f, 2.f, 3.5f, 0.7f, C1);
		C.Line(5.f, 5.f, 4.5f, 2.5f, 0.7f, C1);
	}

	void BubbleShape(FIco& C, const FLinearColor& C1, const FLinearColor& C2)
	{
		C.Disc(8.f, 8.f, 5.6f, C1);
		C.Disc(6.4f, 6.4f, 1.8f, C2);
	}

	void ArmorShape(FIco& C, const FLinearColor& C1, const FLinearColor& C2)
	{
		C.Poly({ { 8.f, 2.5f }, { 14.f, 5.f }, { 14.f, 13.f }, { 8.f, 15.f }, { 2.f, 13.f }, { 2.f, 5.f } }, C1);
		C.Poly({ { 8.f, 4.5f }, { 12.f, 6.f }, { 12.f, 11.5f }, { 8.f, 13.f }, { 4.f, 11.5f }, { 4.f, 6.f } }, C2);
	}

	void ShapeGlyph(FIco& C, FName Glyph)
	{
		const FString N = Glyph.ToString();
		if (N == TEXT("heart_bg")) HeartShape(C, Hex(0x2A2A2A), Hex(0x101010));
		else if (N == TEXT("heart")) HeartShape(C, Red, RedLight);
		else if (N == TEXT("heart_half")) { HeartShape(C, Red, RedLight); for (int32 y = 0; y < FIco::S; ++y) for (int32 x = FIco::S / 2; x < FIco::S; ++x) C.At(x, y) = FLinearColor(0, 0, 0, 0); }
		else if (N == TEXT("heart_poison")) HeartShape(C, Hex(0x6A8A2A), Hex(0x9AC84A));
		else if (N == TEXT("heart_wither")) HeartShape(C, Hex(0x3A3A3A), Hex(0x6A6A6A));
		else if (N == TEXT("heart_absorb")) HeartShape(C, Hex(0xE8E830), Hex(0xFFF890));
		else if (N == TEXT("hunger_bg")) HungerShape(C, Hex(0x2A2A2A), Hex(0x101010));
		else if (N == TEXT("hunger")) HungerShape(C, Hex(0x8A6A3A), Hex(0x5A4020));
		else if (N == TEXT("hunger_half")) { HungerShape(C, Hex(0x8A6A3A), Hex(0x5A4020)); for (int32 y = 0; y < FIco::S; ++y) for (int32 x = 0; x < FIco::S / 2; ++x) C.At(x, y) = FLinearColor(0, 0, 0, 0); }
		else if (N == TEXT("bubble")) BubbleShape(C, White, Hex(0xC8D8F0));
		else if (N == TEXT("bubble_burst")) { BubbleShape(C, Hex(0x5A6A8A), Hex(0x2A3A5A)); C.Line(3.f, 3.f, 13.f, 13.f, 0.8f, White); }
		else if (N == TEXT("armor_bg")) ArmorShape(C, Hex(0x2A2A2A), Hex(0x101010));
		else if (N == TEXT("armor")) ArmorShape(C, Grey, White);
		else if (N == TEXT("xp_bar")) { C.FillRect(0.f, 6.f, 16.f, 10.f, Green); C.FillRect(0.f, 6.f, 16.f, 7.f, Hex(0x9AFF70)); }
		else if (N == TEXT("slot")) { C.FillRect(0.f, 0.f, 16.f, 16.f, Hex(0x8B8B8B, 0.75f)); C.FillRect(1.f, 1.f, 15.f, 15.f, Hex(0x373737, 0.95f)); C.Line(1.f, 1.f, 15.f, 1.f, 0.5f, Hex(0x2A2A2A)); }
		else if (N == TEXT("slot_highlight")) { C.FillRect(0.f, 0.f, 16.f, 16.f, Hex(0xFFFFFF, 0.55f)); }
		else if (N == TEXT("button")) { C.FillRect(0.f, 0.f, 16.f, 16.f, Hex(0x6A6A6A)); C.FillRect(0.f, 0.f, 16.f, 1.5f, Hex(0x8A8A8A)); }
		else if (N == TEXT("crosshair")) { C.Line(8.f, 0.5f, 8.f, 15.5f, 0.9f, White); C.Line(0.5f, 8.f, 15.5f, 8.f, 0.9f, White); }
		else if (N == TEXT("crosshair_bg")) { C.Line(8.f, 0.5f, 8.f, 15.5f, 1.9f, FLinearColor(0, 0, 0, 0.5f)); C.Line(0.5f, 8.f, 15.5f, 8.f, 1.9f, FLinearColor(0, 0, 0, 0.5f)); }
		else if (N == TEXT("glint")) { for (int32 k = 0; k < 22; ++k) C.Disc((float)((k * 7) % 16), (float)((k * 11) % 16), 0.7f, FLinearColor(0.7f, 0.85f, 1.f, 0.85f)); }
		else if (N == TEXT("arrow_up")) { C.Poly({ { 8.f, 2.f }, { 13.f, 8.f }, { 3.f, 8.f } }, White); C.FillRect(6.f, 8.f, 10.f, 13.f, White); }
		else if (N == TEXT("arrow_down")) { C.Poly({ { 8.f, 14.f }, { 13.f, 8.f }, { 3.f, 8.f } }, White); C.FillRect(6.f, 3.f, 10.f, 8.f, White); }
		else if (N == TEXT("search")) { for (int32 a = 0; a < 18; ++a) { const float A0 = a / 18.f * 2 * PI, A1 = (a + 1) / 18.f * 2 * PI; C.Line(6.5f + FMath::Cos(A0) * 4.f, 6.5f + FMath::Sin(A0) * 4.f, 6.5f + FMath::Cos(A1) * 4.f, 6.5f + FMath::Sin(A1) * 4.f, 0.9f, White); } C.Line(10.f, 10.f, 15.f, 15.f, 1.1f, White); }
		else if (N == TEXT("tab")) { C.FillRect(0.f, 0.f, 16.f, 16.f, Hex(0x4A4A4A, 0.6f)); C.FillRect(1.f, 1.f, 15.f, 15.f, Hex(0x6A6A6A, 0.35f)); }
		else if (N == TEXT("tab_selected")) { C.FillRect(0.f, 0.f, 16.f, 16.f, Hex(0xC8C8C8, 0.75f)); C.FillRect(1.f, 1.f, 15.f, 15.f, Hex(0x9A9A9A, 0.45f)); }
		else if (N == TEXT("chest")) { C.FillRect(1.f, 5.f, 15.f, 14.f, Hex(0x8A5A2A)); C.FillRect(2.f, 6.f, 14.f, 9.f, Hex(0xA06A32)); C.FillRect(6.5f, 7.f, 9.5f, 11.f, Hex(0xE8C23A)); }
		else if (N == TEXT("furnace")) { C.FillRect(1.f, 2.f, 15.f, 14.f, Hex(0x8A8A8A)); C.FillRect(4.f, 6.f, 12.f, 12.f, Hex(0x2A2A2A)); C.FillRect(5.f, 7.f, 11.f, 9.f, Hex(0xFF8A20)); }
		else if (N == TEXT("crafting")) { C.FillRect(1.f, 1.f, 15.f, 15.f, Hex(0xB8945F)); C.Line(1.f, 8.f, 15.f, 8.f, 0.6f, Hex(0x6B5335)); C.Line(8.f, 1.f, 8.f, 15.f, 0.6f, Hex(0x6B5335)); }
		else if (N == TEXT("anvil")) { C.FillRect(2.f, 4.f, 14.f, 7.f, Hex(0x4A4A50)); C.FillRect(5.f, 7.f, 11.f, 11.f, Hex(0x5A5A60)); C.FillRect(3.f, 11.f, 13.f, 14.f, Hex(0x4A4A50)); }
		else if (N == TEXT("enchant")) { C.FillRect(2.f, 6.f, 14.f, 14.f, Hex(0x8A1E24)); C.Poly({ { 8.f, 1.f }, { 12.f, 6.f }, { 4.f, 6.f } }, Hex(0x2A1E3A)); C.Disc(8.f, 8.f, 1.4f, Hex(0x3AE0FF)); }
		else if (N == TEXT("potion")) { C.FillRect(7.f, 1.5f, 9.f, 4.f, Hex(0xC8C8C8)); C.Poly({ { 6.f, 4.f }, { 10.f, 4.f }, { 12.f, 9.f }, { 12.f, 13.5f }, { 4.f, 13.5f }, { 4.f, 9.f } }, Hex(0xD03060, 0.85f)); }
		else if (N == TEXT("shulker")) { C.FillRect(2.f, 6.f, 14.f, 14.f, Hex(0x9A7AB8)); C.FillRect(3.f, 4.f, 13.f, 6.f, Hex(0xB89AD0)); }
		else if (N == TEXT("hopper")) { C.FillRect(2.f, 4.f, 14.f, 8.f, Hex(0x5A5A60)); C.Poly({ { 5.f, 8.f }, { 11.f, 8.f }, { 8.f, 14.f } }, Hex(0x4A4A50)); }
		else if (N == TEXT("beacon")) { C.Poly({ { 8.f, 1.f }, { 12.f, 6.f }, { 4.f, 6.f } }, Hex(0x8AF0F0)); C.FillRect(4.f, 6.f, 12.f, 8.f, Hex(0x3A3A44)); C.FillRect(2.f, 8.f, 14.f, 14.f, Hex(0x5A5A66)); }
		else if (N == TEXT("shield")) { C.Poly({ { 3.f, 2.f }, { 13.f, 2.f }, { 13.f, 10.f }, { 8.f, 14.5f }, { 3.f, 10.f } }, Hex(0x8A5A2A)); C.Poly({ { 5.f, 4.f }, { 11.f, 4.f }, { 11.f, 9.5f }, { 8.f, 12.f }, { 5.f, 9.5f } }, Hex(0xC8C8C8)); }
		else if (N == TEXT("map")) { C.FillRect(1.f, 3.f, 15.f, 13.f, Hex(0xE8DCB0)); C.Poly({ { 3.f, 11.f }, { 7.f, 6.f }, { 13.f, 11.f } }, Hex(0x4A8A3A)); }
		else if (N == TEXT("bars")) { C.FillRect(1.f, 4.f, 15.f, 12.f, Hex(0x1E1E1E)); C.FillRect(2.f, 5.f, 14.f, 11.f, Hex(0x6A2A2A)); }
		else if (N == TEXT("lock")) { C.FillRect(3.f, 7.f, 13.f, 14.f, Hex(0xC8A030)); C.FillRect(5.f, 3.f, 11.f, 7.f, Hex(0xA8801E)); C.FillRect(6.5f, 4.f, 9.5f, 7.f, Hex(0x3A3A44)); }
		else if (N == TEXT("plus")) { C.Line(8.f, 3.f, 8.f, 13.f, 1.4f, White); C.Line(3.f, 8.f, 13.f, 8.f, 1.4f, White); }
		else if (N == TEXT("trash")) { C.FillRect(4.f, 3.f, 12.f, 5.f, Grey); C.Poly({ { 4.5f, 5.f }, { 11.5f, 5.f }, { 10.5f, 14.5f }, { 5.5f, 14.5f } }, Hex(0x9A9A9A)); }
		else if (N == TEXT("pickaxe")) { C.Line(3.f, 14.f, 11.f, 5.f, 1.1f, Hex(0x8A6438)); C.Poly({ { 3.f, 3.f }, { 7.f, 1.f }, { 12.f, 1.f }, { 15.f, 5.f }, { 13.f, 8.f }, { 10.f, 4.5f }, { 6.f, 4.f } }, Hex(0xC8C8C8)); }
		else if (N == TEXT("sword")) { C.Line(2.f, 14.f, 5.f, 11.f, 1.2f, Hex(0x8A6438)); C.Poly({ { 4.5f, 9.5f }, { 12.f, 2.f }, { 14.f, 1.5f }, { 13.5f, 3.5f }, { 6.f, 11.f } }, Hex(0xD6D6D6)); }
		else if (N == TEXT("dragon")) { C.Ellipse(8.f, 9.f, 6.f, 3.f, 0.f, Hex(0x1A1A22)); C.Poly({ { 2.f, 6.f }, { 6.f, 3.f }, { 7.f, 8.f } }, Hex(0x2A2A38)); C.Poly({ { 14.f, 6.f }, { 10.f, 3.f }, { 9.f, 8.f } }, Hex(0x2A2A38)); C.Disc(11.f, 6.f, 0.8f, Hex(0xC060FF)); }
		else if (N == TEXT("skull")) { C.Disc(8.f, 7.f, 4.6f, Hex(0xE8E8D8)); C.FillRect(4.f, 8.f, 12.f, 11.f, Hex(0xE8E8D8)); C.FillRect(5.5f, 6.f, 7.5f, 8.f, Dark); C.FillRect(8.5f, 6.f, 10.5f, 8.f, Dark); C.FillRect(7.f, 9.5f, 9.f, 10.5f, Dark); }
		else if (N == TEXT("star")) { for (int32 k = 0; k < 4; ++k) { const float A = k * PI / 2 + PI / 4; C.Poly({ { 8.f + FMath::Cos(A) * 6.f, 8.f + FMath::Sin(A) * 6.f }, { 8.f + FMath::Cos(A + 1.57f) * 1.8f, 8.f + FMath::Sin(A + 1.57f) * 1.8f }, { 8.f + FMath::Cos(A + PI) * 1.6f, 8.f + FMath::Sin(A + PI) * 1.6f }, { 8.f + FMath::Cos(A + 4.71f) * 1.8f, 8.f + FMath::Sin(A + 4.71f) * 1.8f } }, Gold); } }
		else if (N == TEXT("clock")) { C.Disc(8.f, 8.f, 5.6f, Gold); C.Disc(8.f, 8.f, 4.2f, Hex(0x2A3A5A)); C.Line(8.f, 8.f, 8.f, 4.4f, 0.7f, White); C.Line(8.f, 8.f, 11.f, 9.f, 0.7f, White); }
		else if (N == TEXT("sun")) { C.Disc(8.f, 8.f, 3.6f, Gold); for (int32 k = 0; k < 8; ++k) { const float A = k / 8.f * 2 * PI; C.Line(8.f + FMath::Cos(A) * 4.6f, 8.f + FMath::Sin(A) * 4.6f, 8.f + FMath::Cos(A) * 6.6f, 8.f + FMath::Sin(A) * 6.6f, 0.8f, Gold); } }
		else if (N == TEXT("moon")) { C.Disc(8.f, 8.f, 4.6f, Hex(0xE8E8F4)); C.Disc(5.6f, 6.f, 3.f, FLinearColor(0, 0, 0, 0)); }
		else if (N == TEXT("cloud")) { C.Disc(5.5f, 9.f, 3.f, Hex(0xE8E8E8)); C.Disc(9.f, 8.f, 3.6f, Hex(0xF4F4F4)); C.Disc(12.f, 10.f, 2.4f, Hex(0xE0E0E0)); }
		else if (N == TEXT("rain")) { C.Disc(6.f, 6.f, 3.f, Hex(0xB0B0B8)); C.Disc(10.f, 6.5f, 3.4f, Hex(0xC0C0C8)); for (int32 k = 0; k < 4; ++k) C.Line(4.f + k * 3.f, 10.f, 3.f + k * 3.f, 14.f, 0.6f, Hex(0x6090E8)); }
		else if (N == TEXT("thunder")) { C.Disc(6.f, 6.f, 3.f, Hex(0x8A8A94)); C.Disc(10.f, 6.5f, 3.4f, Hex(0x9A9AA4)); C.Poly({ { 9.f, 8.f }, { 5.f, 12.f }, { 8.f, 12.f }, { 6.f, 15.5f }, { 11.f, 11.f }, { 8.f, 11.f }, { 10.f, 8.f } }, Gold); }
		else if (N == TEXT("volume")) { C.Poly({ { 3.f, 6.f }, { 6.f, 6.f }, { 10.f, 2.f }, { 10.f, 14.f }, { 6.f, 10.f }, { 3.f, 10.f } }, White); for (int32 k = 1; k <= 3; ++k) { C.Line(11.f, 8.f - k, 14.f, 8.f - k * 2.f, 0.6f, White); C.Line(11.f, 8.f + k, 14.f, 8.f + k * 2.f, 0.6f, White); } }
		else if (N == TEXT("gear")) { C.Disc(8.f, 8.f, 4.4f, Grey); C.Disc(8.f, 8.f, 1.8f, Dark); for (int32 k = 0; k < 8; ++k) { const float A = k / 8.f * 2 * PI; C.FillRect(8.f + FMath::Cos(A) * 5.4f - 1.f, 8.f + FMath::Sin(A) * 5.4f - 1.f, 8.f + FMath::Cos(A) * 5.4f + 1.f, 8.f + FMath::Sin(A) * 5.4f + 1.f, Grey); } }
		else if (N == TEXT("check")) { for (int32 k = 0; k < 7; ++k) { C.Line(3.f + k * 1.2f, 8.f + k * 0.9f, 4.2f + k * 1.2f, 9.f + k * 0.9f, 1.1f, Green); } for (int32 k = 0; k < 8; ++k) C.Line(11.f + k * 0.6f, 11.f - k * 1.1f, 11.6f + k * 0.6f, 11.9f - k * 1.1f, 1.1f, Green); }
		else if (N == TEXT("cross")) { C.Line(3.f, 3.f, 13.f, 13.f, 1.2f, RedLight); C.Line(13.f, 3.f, 3.f, 13.f, 1.2f, RedLight); }
		else if (N == TEXT("portal")) { C.Ellipse(8.f, 8.f, 4.f, 6.5f, 0.f, Hex(0x7A30D0)); C.Ellipse(8.f, 8.f, 2.4f, 5.f, 0.f, Hex(0xC090FF)); }
		else if (N == TEXT("ender_pearl")) { C.Disc(8.f, 8.f, 4.6f, Hex(0x1A5A4A)); C.Disc(8.f, 8.f, 3.f, Hex(0x2AC0A0)); }
		else if (N == TEXT("eye")) { C.Ellipse(8.f, 8.f, 6.f, 3.6f, 0.f, White); C.Disc(8.f, 8.f, 2.f, Hex(0x2A4A8A)); C.Disc(8.f, 8.f, 1.f, Dark); }
		else if (N == TEXT("tick")) { C.FillRect(0.f, 0.f, 16.f, 16.f, FLinearColor(1, 1, 1, 0.12f)); }
		else { C.FillRect(2.f, 2.f, 14.f, 14.f, Hex(0x9A9A9A, 0.8f)); }
	}
}

namespace MCIcons
{
	const TArray<FName>& GlyphNames()
	{
		static TArray<FName> Names = []()
		{
			TArray<FName> N;
			const TCHAR* L[] = {
				TEXT("heart_bg"), TEXT("heart"), TEXT("heart_half"), TEXT("heart_poison"), TEXT("heart_wither"), TEXT("heart_absorb"),
				TEXT("hunger_bg"), TEXT("hunger"), TEXT("hunger_half"), TEXT("bubble"), TEXT("bubble_burst"), TEXT("armor_bg"), TEXT("armor"),
				TEXT("xp_bar"), TEXT("slot"), TEXT("slot_highlight"), TEXT("button"), TEXT("crosshair"), TEXT("crosshair_bg"), TEXT("glint"),
				TEXT("arrow_up"), TEXT("arrow_down"), TEXT("search"), TEXT("tab"), TEXT("tab_selected"), TEXT("chest"), TEXT("furnace"),
				TEXT("crafting"), TEXT("anvil"), TEXT("enchant"), TEXT("potion"), TEXT("shulker"), TEXT("hopper"), TEXT("beacon"), TEXT("shield"),
				TEXT("map"), TEXT("bars"), TEXT("lock"), TEXT("plus"), TEXT("trash"), TEXT("pickaxe"), TEXT("sword"), TEXT("dragon"), TEXT("skull"),
				TEXT("star"), TEXT("clock"), TEXT("sun"), TEXT("moon"), TEXT("cloud"), TEXT("rain"), TEXT("thunder"), TEXT("volume"), TEXT("gear"),
				TEXT("check"), TEXT("cross"), TEXT("portal"), TEXT("ender_pearl"), TEXT("eye"), TEXT("tick")
			};
			for (const TCHAR* S : L) N.Add(FName(S));
			return N;
		}();
		return Names;
	}

	void PaintGlyph(FName Glyph, FColor* Out)
	{
		FIco C;
		ShapeGlyph(C, Glyph);
		C.Write(Out);
	}
}
