// Procedural item sprites, part 2: food, buckets, potions, books, eggs, discs, redstone parts, vehicles and misc.
#include "Render/MCIcons.h"
#include "Render/MCIconCanvas.h"
#include "Blocks/MCTextureDefsCommon.h"

using namespace MCIconDraw;

namespace MCIconPaint2
{
	using namespace MCIconDraw;

	void Food(FIco& C, const FString& N);
	void Bucket(FIco& C, const FString& N);
	void Potion(FIco& C, const FString& N);
	void Misc(FIco& C, const FString& N, uint32 Seed);

	bool PaintMisc(const FMCItem& I, const FString& N, FIco& C)
	{
		// ---------------- spawn eggs
		if (N.EndsWith(TEXT("_spawn_egg")))
		{
			const FString Mob = N.LeftChop(10);
			const uint32 H = (uint32)GetTypeHash(Mob);
			const FLinearColor Base = Hex(0x3A6A2A + (H % 7) * 0x002A08);
			const FLinearColor Seed = Hex(0xE8E8E8);
			C.Ellipse(8.f, 9.f, 5.f, 6.4f, 0.f, Base);
			C.Ellipse(6.5f, 7.f, 1.8f, 2.6f, 0.f, Sh(Base, 1.25f));
			for (int32 k = 0; k < 7; ++k)
			{
				const float X = 4.f + (float)((H >> (k * 3)) % 10), Y = 5.f + (float)((H >> (k * 2 + 1)) % 10);
				C.Disc(X, Y, 0.8f + (float)((H >> k) % 3) * 0.25f, (k & 1) ? Seed : Sh(Base, 0.6f));
			}
			C.Ellipse(8.f, 12.f, 4.f, 1.6f, 0.f, Sh(Base, 0.75f));
			return true;
		}
		if (I.Kind == EMCItemKind::Dye && I.DyeColor < 16)
		{
			const FLinearColor D = Hex(MCTexDSL::DyeColors[I.DyeColor].Rgb);
			C.Poly({ { 4.f, 6.f }, { 9.f, 3.f }, { 13.f, 6.f }, { 13.5f, 11.f }, { 9.f, 14.f }, { 3.5f, 11.5f } }, D);
			C.Ellipse(7.f, 7.f, 2.f, 1.4f, -0.5f, Sh(D, 1.3f));
			C.Disc(10.5f, 11.f, 1.f, Sh(D, 0.7f));
			return true;
		}
		// ---------------- food & drink
		if (I.Kind == EMCItemKind::Food && I.Food.IsValid())
		{
			Food(C, N);
			return true;
		}
		if (I.Kind == EMCItemKind::Bucket) { Bucket(C, N); return true; }
		if (I.Kind == EMCItemKind::Potion) { Potion(C, N); return true; }
		Misc(C, N, (uint32)GetTypeHash(N));
		return true;
	}

	void Food(FIco& C, const FString& N)
	{
		if (N.Contains(TEXT("apple"))) { C.Ellipse(8.f, 9.f, 4.6f, 4.4f, 0.f, Hex(0xD02A20)); C.Line(8.f, 4.f, 9.f, 2.f, 0.5f, Hex(0x6A4020)); C.Ellipse(11.f, 3.6f, 1.6f, 0.9f, -0.4f, Hex(0x3A8A2A)); C.Ellipse(6.f, 6.5f, 1.2f, 1.6f, 0.f, FLinearColor(1, 1, 1, 0.35f)); return; }
		if (N.Contains(TEXT("bread"))) { C.Poly({ { 2.f, 7.f }, { 5.f, 3.5f }, { 13.f, 4.5f }, { 14.f, 9.f }, { 12.f, 12.f }, { 3.f, 11.f } }, Hex(0xB17A3A)); C.Line(4.5f, 5.f, 12.f, 5.8f, 0.7f, Hex(0xD8A860)); C.Line(4.f, 8.f, 12.5f, 8.8f, 0.6f, Hex(0x8A5A28)); C.Line(4.f, 10.f, 12.f, 10.8f, 0.6f, Hex(0xD8A860)); return; }
		if (N.Contains(TEXT("cookie")) || N == TEXT("pumpkin_pie")) { C.Ellipse(8.f, 8.f, 5.4f, 5.f, 0.f, N == TEXT("cookie") ? Hex(0xB5742A) : Hex(0xE8952A)); for (int32 k = 0; k < 6; ++k) C.Disc(5.f + (k % 3) * 3.f, 6.f + (k / 3) * 3.5f, 0.7f, Hex(0x4A2810)); return; }
		if (N.Contains(TEXT("melon_slice"))) { C.Poly({ { 2.f, 13.f }, { 13.f, 13.f }, { 8.f, 3.f } }, Hex(0xD02A50)); C.Poly({ { 3.4f, 12.f }, { 11.6f, 12.f }, { 8.f, 4.6f } }, Hex(0xF05A78)); for (int32 k = 0; k < 3; ++k) C.Disc(5.5f + k * 2.4f, 10.5f - k * 0.5f, 0.5f, Hex(0x2A1A10)); C.Line(3.f, 12.7f, 13.f, 12.7f, 0.7f, Hex(0x3A8A2A)); return; }
		if (N.Contains(TEXT("kelp"))) { C.Line(6.f, 14.f, 8.f, 4.f, 2.4f, Hex(0x3A5A20)); C.Line(10.f, 14.f, 9.f, 6.f, 2.2f, Hex(0x2E4A18)); C.Line(6.4f, 13.f, 8.2f, 4.5f, 0.7f, Hex(0x5E8A32)); return; }
		if (N.Contains(TEXT("stew")) || N.Contains(TEXT("soup"))) { C.Poly({ { 2.f, 8.f }, { 14.f, 8.f }, { 12.f, 14.f }, { 4.f, 14.f } }, Hex(0x8A5A2A)); C.Ellipse(8.f, 8.f, 6.f, 2.f, 0.f, Hex(0xC88A3A)); C.Ellipse(8.f, 7.8f, 4.6f, 1.4f, 0.f, N.Contains(TEXT("mushroom")) ? Hex(0x8A5A30) : Hex(0xB06A2A)); for (int32 k = 0; k < 3; ++k) C.Disc(6.f + k * 2.f, 7.f, 0.7f, Hex(0xE8D8B0)); return; }
		if (N.Contains(TEXT("honey_bottle"))) { C.FillRect(6.f, 4.f, 10.f, 14.f, Hex(0xE8A020, 0.85f)); C.FillRect(7.f, 2.f, 9.f, 4.f, Hex(0xC8C8C8)); C.FillRect(6.5f, 4.5f, 7.5f, 13.f, Hex(0xFFD080)); return; }
		if (N.Contains(TEXT("dried_kelp"))) { C.Poly({ { 3.f, 12.f }, { 5.f, 4.f }, { 11.f, 4.f }, { 13.f, 12.f } }, Hex(0x2E3A1A)); for (int32 k = 0; k < 4; ++k) C.Line(4.5f + k * 2.f, 5.f, 4.5f + k * 2.f, 11.5f, 0.5f, Hex(0x4A5A2A)); return; }
		if (N.Contains(TEXT("berries"))) { for (int32 k = 0; k < 5; ++k) C.Disc(5.f + (k % 3) * 3.4f, 6.f + (k / 3) * 3.6f, 1.7f, N.StartsWith(TEXT("glow")) ? Hex(0xE8A020) : Hex(0xB01A2A)); C.Line(4.f, 3.f, 12.f, 3.f, 0.5f, Hex(0x3A6A2A)); return; }
		if (N.Contains(TEXT("chicken")) || N.Contains(TEXT("beef")) || N.Contains(TEXT("porkchop")) || N.Contains(TEXT("mutton")) || N.Contains(TEXT("rabbit")))
		{
			const bool bCooked = N.StartsWith(TEXT("cooked"));
			const FLinearColor M = bCooked ? Hex(0xB5642E) : Hex(0xD89A9A);
			C.Poly({ { 3.f, 11.f }, { 4.f, 6.f }, { 9.f, 3.f }, { 13.f, 7.f }, { 12.5f, 11.f }, { 8.f, 13.5f } }, M);
			C.Line(5.f, 6.f, 11.f, 9.f, 0.6f, bCooked ? Hex(0x8A4418) : Hex(0xB86A6A));
			C.Line(5.f, 8.5f, 11.5f, 11.f, 0.6f, bCooked ? Hex(0x8A4418) : Hex(0xB86A6A));
			if (N.Contains(TEXT("rabbit")) || N.Contains(TEXT("chicken"))) { C.Line(12.5f, 8.f, 14.f, 6.f, 0.7f, Hex(0xF0F0F0)); C.Disc(14.f, 5.5f, 0.9f, Hex(0xF0F0F0)); }
			return;
		}
		if (N.Contains(TEXT("cod")) || N.Contains(TEXT("salmon")) || N.Contains(TEXT("tropical_fish")) || N.Contains(TEXT("pufferfish")))
		{
			const bool bCooked = N.StartsWith(TEXT("cooked"));
			const FLinearColor M = bCooked ? Hex(0xC08A4A) : (N.Contains(TEXT("salmon")) ? Hex(0xE87858) : Hex(0x8A9AA8));
			C.Ellipse(9.f, 7.f, 5.4f, 2.8f, -0.25f, M);
			C.Poly({ { 3.5f, 7.f }, { 1.f, 4.f }, { 1.f, 10.f } }, Sh(M, 0.85f));
			C.Disc(12.5f, 5.8f, 0.5f, Hex(0x101010));
			return;
		}
		if (N.Contains(TEXT("carrot"))) { C.Line(6.f, 3.f, 10.f, 13.f, 3.f, Hex(0xE8952A)); C.Poly({ { 5.f, 12.f }, { 10.5f, 12.f }, { 7.5f, 15.f } }, Hex(0xE8952A)); C.Line(7.f, 3.f, 6.f, 1.f, 0.7f, Hex(0x3A8A2A)); C.Line(8.f, 3.f, 9.f, 1.f, 0.7f, Hex(0x3A8A2A)); return; }
		if (N.Contains(TEXT("potato"))) { C.Ellipse(8.f, 9.f, 5.f, 3.6f, 0.3f, Hex(0xC8A060)); for (int32 k = 0; k < 5; ++k) C.Disc(5.f + (k % 3) * 3.f, 7.f + (k / 3) * 3.f, 0.5f, Hex(0x8A6A38)); return; }
		if (N.Contains(TEXT("beetroot"))) { C.Ellipse(8.f, 10.f, 4.f, 4.4f, 0.f, Hex(0x9A1A38)); C.Poly({ { 7.f, 2.f }, { 9.f, 6.f }, { 7.f, 6.f } }, Hex(0x3A8A2A)); C.Poly({ { 5.f, 3.f }, { 8.f, 6.5f }, { 6.f, 6.5f } }, Hex(0x4A9A2A)); C.Poly({ { 11.f, 3.f }, { 10.f, 6.5f }, { 8.f, 6.5f } }, Hex(0x4A9A2A)); C.Line(8.f, 7.f, 8.f, 12.f, 0.4f, Hex(0x6A0A22)); return; }
		if (N == TEXT("sweet_berries") || N == TEXT("glow_berries")) { for (int32 k = 0; k < 5; ++k) C.Disc(5.f + (k % 3) * 3.4f, 6.f + (k / 3) * 3.6f, 1.7f, N.StartsWith(TEXT("glow")) ? Hex(0xF0A020) : Hex(0xC01A2A)); return; }
		if (N == TEXT("rotten_flesh")) { C.Poly({ { 3.f, 10.f }, { 5.f, 5.f }, { 10.f, 4.f }, { 13.f, 8.f }, { 11.f, 13.f }, { 6.f, 12.f } }, Hex(0x6A4A3A)); for (int32 k = 0; k < 4; ++k) C.Disc(6.f + k * 1.8f, 7.f + (k & 1) * 2.f, 0.6f, Hex(0x3A2A20)); return; }
		if (N == TEXT("spider_eye")) { C.Disc(8.f, 9.f, 5.f, Hex(0x8A2A2A)); C.Disc(8.f, 9.f, 2.4f, Hex(0xD04A3A)); C.Disc(8.f, 9.f, 1.f, Hex(0x2A0A0A)); return; }
		if (N == TEXT("chorus_fruit")) { C.Ellipse(8.f, 9.f, 4.f, 4.6f, 0.f, Hex(0x9A6A9A)); for (int32 k = 0; k < 8; ++k) { const float A = k / 8.f * 2 * PI; C.Disc(8.f + FMath::Cos(A) * 2.6f, 9.f + FMath::Sin(A) * 3.f, 0.8f, Hex(0xD8B0D8)); } return; }
		if (N == TEXT("pufferfish")) { C.Ellipse(8.f, 9.f, 5.f, 5.f, 0.f, Hex(0xE8C040)); for (int32 k = 0; k < 10; ++k) { const float A = k / 10.f * 2 * PI; C.Line(8.f + FMath::Cos(A) * 3.6f, 9.f + FMath::Sin(A) * 3.6f, 8.f + FMath::Cos(A) * 6.f, 9.f + FMath::Sin(A) * 6.f, 0.6f, Hex(0xC89A20)); } C.Disc(11.5f, 7.f, 0.6f, Hex(0x101010)); return; }
		// generic food fallback: a bowl/bun shape
		C.Ellipse(8.f, 9.f, 5.f, 4.f, 0.f, Hex(0xC88A3A));
		C.Disc(6.f, 7.5f, 0.9f, Hex(0xF0D8A0));
	}

	void Bucket(FIco& C, const FString& N)
	{
		const FLinearColor Metal = Hex(0xC8C8C8), Dark = Hex(0x8A8A8A);
		if (N == TEXT("water_bucket")) { C.Poly({ { 3.f, 6.f }, { 13.f, 6.f }, { 11.5f, 14.f }, { 4.5f, 14.f } }, Hex(0x3A7AE8, 0.9f)); }
		else if (N == TEXT("lava_bucket")) { C.Poly({ { 3.f, 6.f }, { 13.f, 6.f }, { 11.5f, 14.f }, { 4.5f, 14.f } }, Hex(0xE8520A)); C.Poly({ { 4.f, 8.f }, { 12.f, 8.f }, { 11.f, 13.f }, { 5.f, 13.f } }, Hex(0xFFB030)); }
		else if (N == TEXT("milk_bucket")) { C.Poly({ { 3.f, 6.f }, { 13.f, 6.f }, { 11.5f, 14.f }, { 4.5f, 14.f } }, Hex(0xF4F4F4)); }
		else if (N == TEXT("powder_snow_bucket")) { C.Poly({ { 3.f, 6.f }, { 13.f, 6.f }, { 11.5f, 14.f }, { 4.5f, 14.f } }, Hex(0xEEF4F8)); }
		else if (N.EndsWith(TEXT("_bucket")))
		{
			const FString Mob = N.LeftChop(7);
			const uint32 H = (uint32)GetTypeHash(Mob);
			C.Poly({ { 3.f, 6.f }, { 13.f, 6.f }, { 11.5f, 14.f }, { 4.5f, 14.f } }, Hex(0x3A7AE8, 0.85f));
			const FLinearColor M = Hex(0x8A6A4A + (H % 5) * 0x001008);
			C.Ellipse(8.f, 10.f, 3.2f, 2.f, 0.f, M);
			C.Poly({ { 5.f, 10.f }, { 11.f, 10.f }, { 9.f, 7.f } }, M);
			C.Disc(9.5f, 9.f, 0.35f, Hex(0x101010));
		}
		// bucket body
		C.Line(3.5f, 6.5f, 12.5f, 6.5f, 1.f, Metal);
		C.Line(3.5f, 13.5f, 12.5f, 13.5f, 1.2f, Dark);
		C.Line(4.2f, 7.f, 5.2f, 13.5f, 0.6f, Sh(Dark, 1.2f));
		C.Line(11.8f, 7.f, 10.8f, 13.5f, 0.6f, Sh(Dark, 1.2f));
		C.Line(4.4f, 6.4f, 11.6f, 6.4f, 0.5f, Sh(Metal, 1.2f));
		// handle
		for (int32 a = 0; a < 14; ++a) { const float A0 = PI + a / 14.f * PI, A1 = PI + (a + 1) / 14.f * PI; C.Line(8.f + FMath::Cos(A0) * 4.2f, 6.2f + FMath::Sin(A0) * 2.6f, 8.f + FMath::Cos(A1) * 4.2f, 6.2f + FMath::Sin(A1) * 2.6f, 0.5f, Sh(Metal, 1.1f)); }
	}

	void Potion(FIco& C, const FString& N)
	{
		const bool bSplash = N.StartsWith(TEXT("splash"));
		const bool bLingering = N.StartsWith(TEXT("lingering"));
		const uint32 H = (uint32)GetTypeHash(N);
		const FLinearColor Liquid = Hex(0x4060C8 + ((H >> 3) % 6) * 0x001040);
		C.FillRect(7.f, 1.5f, 9.f, 3.5f, Hex(0xC8C8C8));
		C.Poly({ { 6.5f, 3.5f }, { 9.5f, 3.5f }, { 9.5f, 6.f }, { 11.5f, 10.f }, { 11.5f, 13.5f }, { 4.5f, 13.5f }, { 4.5f, 10.f }, { 6.5f, 6.f } }, Hex(0xD8E8F0, 0.45f));
		C.Poly({ { 6.8f, 6.5f }, { 9.2f, 6.5f }, { 11.f, 10.f }, { 11.f, 13.2f }, { 5.f, 13.2f }, { 5.f, 10.f } }, Liquid);
		for (int32 k = 0; k < 3; ++k) C.Disc(6.f + k * 1.6f, 8.5f + (k & 1) * 1.5f, 0.5f, FLinearColor(1, 1, 1, 0.5f));
		if (bSplash || bLingering) { C.FillRect(5.5f, 3.5f, 10.5f, 4.5f, bLingering ? Hex(0x9A6AD8) : Hex(0xC86868)); if (bLingering) { C.Line(7.f, 2.f, 6.f, 0.8f, 0.4f, Hex(0x9A6AD8)); C.Line(9.f, 2.f, 10.f, 0.8f, 0.4f, Hex(0x9A6AD8)); } }
	}

	void Misc(FIco& C, const FString& N, uint32 Seed)
	{
		// paper & books
		if (N == TEXT("paper")) { C.Poly({ { 3.f, 3.f }, { 13.f, 3.f }, { 13.f, 13.f }, { 3.f, 13.f } }, Hex(0xF0F0E8)); C.Line(4.f, 5.5f, 12.f, 5.5f, 0.5f, Hex(0xB8B8B0)); C.Line(4.f, 7.5f, 12.f, 7.5f, 0.5f, Hex(0xB8B8B0)); C.Line(4.f, 9.5f, 10.f, 9.5f, 0.5f, Hex(0xB8B8B0)); return; }
		if (N.Contains(TEXT("book"))) { C.FillRect(3.f, 2.5f, 13.f, 13.5f, N.Contains(TEXT("enchanted")) ? Hex(0x9A6AD8) : Hex(0x6A4A2A)); C.FillRect(4.5f, 3.5f, 13.f, 12.5f, Hex(0xF0E8D8)); C.Line(8.f, 3.5f, 8.f, 12.5f, 0.5f, Hex(0xB0A890)); return; }
		if (N == TEXT("map") || N == TEXT("filled_map")) { C.FillRect(2.5f, 3.f, 13.5f, 13.f, Hex(0xE8DCB0)); C.Line(3.5f, 5.f, 9.f, 5.f, 0.4f, Hex(0x8A7A5A)); C.Line(3.5f, 7.5f, 12.f, 7.5f, 0.4f, Hex(0x8A7A5A)); C.Line(3.5f, 10.f, 8.f, 10.f, 0.4f, Hex(0x8A7A5A)); if (N == TEXT("filled_map")) { C.Poly({ { 5.f, 11.f }, { 9.f, 8.f }, { 12.f, 11.f } }, Hex(0x4A8A3A)); C.Disc(9.f, 6.f, 0.7f, Hex(0xD02A20)); } return; }
		// redstone parts
		if (N == TEXT("repeater") || N == TEXT("comparator"))
		{
			C.FillRect(2.f, 8.f, 14.f, 12.f, Hex(0xB0B0B0));
			C.FillRect(3.f, 9.f, 13.f, 11.f, Hex(0xD8D8D8));
			C.Line(7.f, 8.5f, 9.f, 8.5f, 1.2f, Hex(0x8A8A8A));
			if (N == TEXT("repeater")) { C.Disc(6.f, 10.f, 1.1f, Hex(0xD03020)); C.Disc(10.f, 10.f, 1.1f, Hex(0xD03020)); }
			else { C.Disc(5.5f, 10.f, 1.1f, Hex(0xD03020)); C.Disc(8.f, 10.f, 1.1f, Hex(0xD03020)); C.Disc(10.5f, 10.f, 1.1f, Hex(0xD03020)); }
			return;
		}
		if (N == TEXT("lever")) { C.FillRect(2.f, 11.f, 14.f, 14.f, Hex(0x8A8A8A)); C.Line(8.f, 12.f, 12.f, 3.f, 1.2f, Hex(0x9A7446)); C.Disc(12.f, 2.5f, 1.2f, Hex(0x6E5230)); return; }
		if (N == TEXT("redstone_torch") || N == TEXT("torch") || N == TEXT("soul_torch")) { C.Line(8.f, 4.f, 8.f, 14.f, 2.2f, Hex(0x7A5A30)); C.Line(7.6f, 4.f, 7.6f, 14.f, 0.7f, Hex(0xA07E4A)); const FLinearColor F = N == TEXT("redstone_torch") ? Hex(0xD02020) : (N == TEXT("soul_torch") ? Hex(0x40E0E8) : Hex(0xFFB020)); C.Disc(8.f, 3.5f, 1.6f, F); return; }
		if (N == TEXT("string")) { for (int32 k = 0; k < 4; ++k) { const float A = k * PI / 4; C.Line(8.f - FMath::Cos(A) * 6, 8.f - FMath::Sin(A) * 6, 8.f + FMath::Cos(A) * 6, 8.f + FMath::Sin(A) * 6, 0.5f, Hex(0xE8E8E8)); } return; }
		if (N == TEXT("feather")) { C.Poly({ { 3.f, 14.f }, { 6.f, 6.f }, { 12.f, 2.f }, { 12.f, 8.f }, { 7.f, 13.f } }, Hex(0xF0F0F0)); C.Line(3.5f, 13.5f, 12.f, 2.5f, 0.5f, Hex(0xC0C0C0)); return; }
		if (N == TEXT("leather") || N == TEXT("rabbit_hide")) { C.Poly({ { 3.f, 10.f }, { 4.f, 5.f }, { 8.f, 3.5f }, { 12.f, 5.f }, { 13.5f, 10.f }, { 8.f, 13.f } }, Hex(0x9A6A3A)); C.Line(5.f, 8.f, 11.f, 8.f, 0.4f, Hex(0x6A4520)); return; }
		if (N == TEXT("flint")) { C.Poly({ { 3.f, 11.f }, { 6.f, 4.f }, { 12.f, 5.f }, { 13.f, 11.f }, { 8.f, 13.f } }, Hex(0x4A4A50)); C.Line(6.f, 5.f, 11.5f, 6.f, 0.5f, Hex(0x7A7A80)); return; }
		if (N == TEXT("clay_ball") || N == TEXT("brick") || N == TEXT("nether_brick")) { const FLinearColor M = N == TEXT("clay_ball") ? Hex(0x9AA4B0) : (N == TEXT("brick") ? Hex(0x9A4E3A) : Hex(0x3A1C21)); C.Ellipse(8.f, 9.f, 4.4f, 3.6f, 0.f, M); C.Ellipse(7.f, 8.f, 1.6f, 1.2f, 0.f, Sh(M, 1.2f)); return; }
		if (N == TEXT("sugar") || N == TEXT("glowstone_dust") || N == TEXT("redstone") || N.Contains(TEXT("dust"))) { for (int32 k = 0; k < 14; ++k) C.Disc(4.f + (float)((Seed + k * 7) % 9), 5.f + (float)((Seed + k * 13) % 9), 0.7f, Hex(0xD8D0C0)); return; }
		if (N == TEXT("slime_ball")) { C.Disc(8.f, 9.f, 4.4f, Hex(0x6AC050)); C.Disc(6.5f, 7.5f, 1.4f, Hex(0x9AE880)); return; }
		if (N == TEXT("honeycomb")) { C.Poly({ { 4.f, 5.f }, { 12.f, 5.f }, { 14.f, 9.f }, { 12.f, 13.f }, { 4.f, 13.f }, { 2.f, 9.f } }, Hex(0xE8A020)); C.Poly({ { 5.5f, 6.5f }, { 10.5f, 6.5f }, { 12.f, 9.f }, { 10.5f, 11.5f }, { 5.5f, 11.5f }, { 4.f, 9.f } }, Hex(0xFFC850)); C.Line(6.f, 9.f, 10.f, 9.f, 0.4f, Hex(0xC88010)); return; }
		if (N == TEXT("ender_pearl") || N == TEXT("ender_eye")) { C.Disc(8.f, 9.f, 4.6f, Hex(0x1A5A4A)); C.Disc(8.f, 9.f, 3.f, Hex(0x2AC0A0)); C.Disc(7.f, 8.f, 1.2f, Hex(0x9AFFF0)); if (N == TEXT("ender_eye")) { C.Line(4.f, 5.f, 8.f, 2.f, 0.5f, Hex(0x8A6A3A)); C.Line(12.f, 5.f, 8.f, 2.f, 0.5f, Hex(0x8A6A3A)); } return; }
		if (N == TEXT("blaze_rod") || N == TEXT("breeze_rod")) { C.Line(4.f, 14.f, 12.f, 3.f, 2.2f, N.StartsWith(TEXT("blaze")) ? Hex(0xE8A020) : Hex(0x8AA0E0)); C.Line(4.2f, 13.f, 11.8f, 3.5f, 0.8f, N.StartsWith(TEXT("blaze")) ? Hex(0xFFF0A0) : Hex(0xE0E8FF)); return; }
		if (N == TEXT("ghast_tear")) { C.Poly({ { 8.f, 2.f }, { 11.f, 8.f }, { 8.f, 14.f }, { 5.f, 8.f } }, Hex(0xE8F4F4)); C.Disc(7.f, 8.f, 1.f, FLinearColor(1, 1, 1, 0.6f)); return; }
		if (N == TEXT("magma_cream")) { C.Disc(8.f, 9.f, 4.6f, Hex(0x8A3A10)); for (int32 k = 0; k < 5; ++k) C.Disc(5.5f + (k % 3) * 2.6f, 7.f + (k / 3) * 3.2f, 0.8f, Hex(0xFF7A1A)); return; }
		if (N == TEXT("nether_star")) { for (int32 k = 0; k < 4; ++k) { const float A = k * PI / 2; C.Poly({ { 8.f + FMath::Cos(A) * 6.f, 8.f + FMath::Sin(A) * 6.f }, { 8.f + FMath::Cos(A + 1.57f) * 1.8f, 8.f + FMath::Sin(A + 1.57f) * 1.8f }, { 8.f + FMath::Cos(A + PI) * 1.6f, 8.f + FMath::Sin(A + PI) * 1.6f }, { 8.f + FMath::Cos(A + 4.71f) * 1.8f, 8.f + FMath::Sin(A + 4.71f) * 1.8f } }, Hex(0xF0F0F0)); } C.Disc(8.f, 8.f, 1.6f, Hex(0x8080FF)); return; }
		if (N == TEXT("shulker_shell")) { C.Poly({ { 3.f, 12.f }, { 4.f, 5.f }, { 12.f, 5.f }, { 13.f, 12.f } }, Hex(0x9A7AB8)); for (int32 k = 0; k < 4; ++k) C.Line(5.f + k * 2.f, 6.f, 5.f + k * 2.f, 11.5f, 0.4f, Hex(0x6A4A88)); C.FillRect(5.f, 4.f, 11.f, 5.f, Hex(0xB89AD0)); return; }
		if (N == TEXT("phantom_membrane")) { C.Poly({ { 2.f, 8.f }, { 8.f, 4.f }, { 14.f, 8.f }, { 8.f, 12.f } }, Hex(0xE8D8A8)); C.Line(3.f, 8.f, 13.f, 8.f, 0.4f, Hex(0xB8A878)); return; }
		if (N == TEXT("scute") || N == TEXT("turtle_scute")) { C.Poly({ { 4.f, 7.f }, { 8.f, 4.f }, { 12.f, 7.f }, { 12.f, 11.f }, { 8.f, 14.f }, { 4.f, 11.f } }, Hex(0x4A9A3A)); C.Poly({ { 6.f, 8.f }, { 8.f, 6.5f }, { 10.f, 8.f }, { 10.f, 10.f }, { 8.f, 11.5f }, { 6.f, 10.f } }, Hex(0x7AC85A)); return; }
		if (N.Contains(TEXT("shell")) && !N.Contains(TEXT("shulker"))) { C.Ellipse(8.f, 8.f, 5.f, 4.4f, 0.f, Hex(0xE8D8B0)); for (int32 k = 0; k < 3; ++k) C.Line(5.f + k * 3.f, 5.f, 5.f + k * 3.f, 11.f, 0.4f, Hex(0xB8A878)); return; }
		if (N == TEXT("heart_of_the_sea")) { C.Poly({ { 8.f, 4.f }, { 11.f, 6.5f }, { 8.f, 13.f }, { 5.f, 6.5f } }, Hex(0x2A8A9A)); C.Disc(8.f, 7.f, 1.6f, Hex(0x60E0E8)); return; }
		if (N == TEXT("nautilus_shell")) { C.Ellipse(8.f, 8.f, 5.4f, 4.6f, 0.4f, Hex(0xE8D0C0)); for (int32 k = 0; k < 5; ++k) C.Line(8.f + k * 0.6f, 8.f - k * 0.6f, 12.f - k * 0.4f, 8.f + k * 1.2f, 0.4f, Hex(0xC0A090)); return; }
		if (N == TEXT("trial_key") || N == TEXT("ominous_trial_key")) { C.FillRect(3.f, 6.f, 13.f, 10.f, Hex(0xC8A030)); C.FillRect(11.f, 7.f, 15.f, 9.f, Hex(0xA8801E)); C.Disc(5.5f, 8.f, 1.4f, Hex(0x7A5A10)); if (N.StartsWith(TEXT("ominous"))) C.FillRect(3.f, 6.f, 13.f, 7.f, Hex(0x5A2A8A)); return; }
		if (N == TEXT("heavy_core")) { for (int32 y = 0; y < FIco::S; ++y) for (int32 x = 0; x < FIco::S; ++x) { const float dx = x / 2.f + 0.5f - 8.f, dy = y / 2.f + 0.5f - 8.5f; if (dx * dx + dy * dy > 30.f) continue; C.Set(x, y, Sh(Hex(0x9A9AA8), 0.8f + ((x + y) % 4 == 0 ? 0.3f : 0.f))); } C.FillRect(5.f, 8.f, 11.f, 11.f, Hex(0x6A6A78)); return; }
		if (N == TEXT("resin_clump") || N == TEXT("resin_brick")) { const FLinearColor R = Hex(0xD9661E); C.Poly({ { 4.f, 11.f }, { 5.5f, 5.f }, { 10.f, 4.f }, { 12.5f, 9.f }, { 10.f, 13.f } }, R); C.Disc(7.f, 7.5f, 1.f, Hex(0xF0A050)); return; }
		if (N == TEXT("fermented_spider_eye")) { C.Disc(8.f, 9.f, 4.6f, Hex(0x4A2A6A)); C.Disc(8.f, 9.f, 2.2f, Hex(0x8A5AA8)); C.Disc(8.f, 9.f, 0.9f, Hex(0x1A0A2A)); return; }
		if (N == TEXT("glistering_melon_slice")) { C.Poly({ { 2.f, 13.f }, { 13.f, 13.f }, { 8.f, 3.f } }, Hex(0xE8C840)); C.Poly({ { 3.4f, 12.f }, { 11.6f, 12.f }, { 8.f, 4.6f } }, Hex(0xFFE870)); for (int32 k = 0; k < 3; ++k) C.Disc(5.5f + k * 2.4f, 10.5f - k * 0.5f, 0.5f, Hex(0xF0A020)); return; }
		if (N == TEXT("echo_shard")) { C.Poly({ { 6.f, 13.f }, { 4.f, 6.f }, { 8.f, 2.f }, { 12.f, 6.f }, { 10.f, 13.f } }, Hex(0x1A4A5A)); C.Line(8.f, 3.f, 7.f, 12.f, 0.6f, Hex(0x3AE0F0)); return; }
		if (N == TEXT("disc_fragment_5")) { for (int32 a = 0; a < 12; ++a) { const float A0 = a / 12.f * PI, A1 = (a + 1) / 12.f * PI; C.Line(8.f + FMath::Cos(A0) * 5.f, 8.f + FMath::Sin(A0) * 5.f, 8.f + FMath::Cos(A1) * 5.f, 8.f + FMath::Sin(A1) * 5.f, 1.6f, Hex(0x4A4A5A)); } return; }
		if (N.StartsWith(TEXT("music_disc"))) { C.Disc(8.f, 8.f, 6.f, Hex(0x2A2A2A)); C.Disc(8.f, 8.f, 2.2f, N.Contains(TEXT("opus55")) ? Hex(0xE8A020) : Hex(0x40A0C0)); C.Disc(8.f, 8.f, 0.8f, Hex(0x101010)); for (int32 a = 0; a < 16; ++a) { const float A = a / 16.f * 2 * PI; C.Disc(8.f + FMath::Cos(A) * 4.2f, 8.f + FMath::Sin(A) * 4.2f, 0.3f, Hex(0x606060)); } return; }
		if (N == TEXT("gunpowder")) { for (int32 k = 0; k < 18; ++k) C.Disc(3.f + (float)((Seed + k * 5) % 11), 4.f + (float)((Seed + k * 9) % 10), 0.6f, Hex(0x5A5A5A)); return; }
		if (N == TEXT("brick")) { C.FillRect(3.f, 6.f, 13.f, 10.f, Hex(0x9A4E3A)); return; }
		// tools/vehicles/misc gadgets
		if (N == TEXT("shears")) { C.Line(3.f, 13.f, 9.f, 6.f, 1.2f, Hex(0xC8C8C8)); C.Line(4.f, 14.f, 8.f, 8.f, 1.2f, Hex(0xA8A8A8)); C.Disc(10.f, 5.f, 1.4f, Hex(0xD8D8D8)); C.Disc(6.f, 4.5f, 1.4f, Hex(0xD8D8D8)); C.Line(10.f, 5.f, 12.5f, 3.f, 1.f, Hex(0xC8C8C8)); C.Line(6.f, 4.5f, 3.5f, 2.5f, 1.f, Hex(0xC8C8C8)); return; }
		if (N == TEXT("flint_and_steel")) { C.FillRect(3.f, 8.f, 12.f, 12.f, Hex(0x8A8A8A)); C.Poly({ { 8.f, 8.f }, { 13.f, 3.f }, { 15.f, 5.f }, { 11.f, 10.f } }, Hex(0xC0C0C0)); C.Disc(5.f, 6.5f, 1.6f, Hex(0x4A4A50)); return; }
		if (N == TEXT("compass") || N == TEXT("recovery_compass")) { C.Disc(8.f, 8.f, 6.f, Hex(0x8A8A8A)); C.Disc(8.f, 8.f, 5.f, Hex(0x2A2A2A)); C.Poly({ { 8.f, 4.f }, { 10.f, 8.f }, { 8.f, 7.f } }, N.StartsWith(TEXT("recovery")) ? Hex(0x60E0A0) : Hex(0xD03020)); C.Poly({ { 8.f, 12.f }, { 6.f, 8.f }, { 8.f, 9.f } }, Hex(0xF0F0F0)); return; }
		if (N == TEXT("clock")) { C.Disc(8.f, 8.f, 6.f, Hex(0xC8A030)); C.Disc(8.f, 8.f, 4.6f, Hex(0x2A3A5A)); C.Line(8.f, 8.f, 8.f, 4.5f, 0.7f, Hex(0xF0F0F0)); C.Line(8.f, 8.f, 11.f, 9.f, 0.7f, Hex(0xF0F0F0)); return; }
		if (N == TEXT("spyglass")) { C.Poly({ { 3.f, 13.f }, { 5.f, 11.f }, { 12.f, 3.f }, { 14.f, 5.f }, { 7.f, 13.f } }, Hex(0x6A5A3A)); C.Poly({ { 12.f, 3.f }, { 14.f, 5.f }, { 15.f, 4.f }, { 13.f, 2.f } }, Hex(0x4A8AE8)); return; }
		if (N == TEXT("lead")) { for (int32 a = 0; a < 20; ++a) { const float A = a / 20.f * 2 * PI; C.Disc(8.f + FMath::Cos(A) * 4.f, 8.f + FMath::Sin(A) * 4.f, 0.6f, Hex(0x8A6A3A)); } return; }
		if (N == TEXT("name_tag")) { C.FillRect(2.f, 6.f, 10.f, 11.f, Hex(0xE8E0D0)); C.FillRect(10.f, 7.f, 14.f, 10.f, Hex(0xD8D0C0)); C.Disc(4.f, 8.5f, 1.f, Hex(0x8A8A8A)); return; }
		if (N == TEXT("saddle")) { C.Poly({ { 2.f, 11.f }, { 5.f, 6.f }, { 11.f, 6.f }, { 14.f, 11.f }, { 11.f, 13.f }, { 5.f, 13.f } }, Hex(0x8A5A2A)); C.FillRect(3.f, 10.f, 13.f, 11.5f, Hex(0x5A3A18)); return; }
		if (N.EndsWith(TEXT("_harness"))) { const uint32 H = (uint32)GetTypeHash(N); const FLinearColor M = Hex(0xE9ECEC + ((H >> 2) % 5) * 0x001020); C.Poly({ { 3.f, 8.f }, { 13.f, 8.f }, { 13.f, 12.f }, { 3.f, 12.f } }, M); C.Poly({ { 5.f, 8.f }, { 11.f, 4.f }, { 12.f, 8.f } }, Sh(M, 0.8f)); return; }
		if (N == TEXT("totem_of_undying")) { const FLinearColor G = Hex(0xE8C23A), T = Hex(0x2AC0A0); C.Poly({ { 8.f, 2.f }, { 13.f, 6.f }, { 11.f, 14.f }, { 5.f, 14.f }, { 3.f, 6.f } }, G); C.Disc(8.f, 7.f, 1.2f, T); C.Disc(6.5f, 6.f, 0.5f, T); C.Disc(9.5f, 6.f, 0.5f, T); C.Line(8.f, 9.f, 8.f, 12.f, 0.6f, T); C.Line(6.f, 10.5f, 10.f, 10.5f, 0.6f, T); return; }
		if (N == TEXT("firework_rocket") || N == TEXT("firework_star")) { if (N.EndsWith(TEXT("star"))) { for (int32 k = 0; k < 6; ++k) { const float A = k / 6.f * 2 * PI; C.Poly({ { 8.f + FMath::Cos(A) * 6.f, 8.f + FMath::Sin(A) * 6.f }, { 8.f + FMath::Cos(A + 2.1f) * 2.f, 8.f + FMath::Sin(A + 2.1f) * 2.f }, { 8.f + FMath::Cos(A + 4.2f) * 2.f, 8.f + FMath::Sin(A + 4.2f) * 2.f } }, Hex(0xE8E8E8)); } C.Disc(8.f, 8.f, 1.2f, Hex(0xD03020)); } else { C.FillRect(7.f, 3.f, 9.f, 12.f, Hex(0xD8D8D8)); C.Poly({ { 7.f, 12.f }, { 9.f, 12.f }, { 10.f, 14.f }, { 6.f, 14.f } }, Hex(0xB04A20)); C.Line(7.4f, 5.f, 7.4f, 10.f, 0.4f, Hex(0xFFE080)); } return; }
		if (N == TEXT("fire_charge")) { C.Disc(8.f, 9.f, 4.4f, Hex(0xE8A020)); C.Disc(7.f, 8.f, 1.8f, Hex(0xFFF0A0)); C.Line(5.f, 4.f, 8.f, 2.f, 0.5f, Hex(0x8A6A3A)); return; }
		if (N == TEXT("snowball")) { C.Disc(8.f, 9.f, 4.6f, Hex(0xF0F4F8)); C.Disc(6.6f, 7.6f, 1.4f, FLinearColor(1, 1, 1, 1)); return; }
		if (N == TEXT("egg")) { C.Ellipse(8.f, 9.f, 3.8f, 5.f, 0.f, Hex(0xF0E8D8)); C.Disc(6.5f, 7.f, 1.1f, Hex(0xC8B8A0)); C.Disc(9.5f, 10.f, 0.9f, Hex(0xC8B8A0)); return; }
		if (N == TEXT("experience_bottle")) { C.FillRect(7.f, 1.5f, 9.f, 4.f, Hex(0xC8C8C8)); C.Poly({ { 6.f, 4.f }, { 10.f, 4.f }, { 12.f, 9.f }, { 12.f, 13.5f }, { 4.f, 13.5f }, { 4.f, 9.f } }, Hex(0x80E030, 0.8f)); for (int32 k = 0; k < 4; ++k) C.Disc(6.f + (k % 2) * 4.f, 7.f + (k / 2) * 4.f, 0.6f, Hex(0xE0FFB0)); return; }
		if (N == TEXT("arrow") || N == TEXT("spectral_arrow") || N == TEXT("tipped_arrow")) { C.Line(3.f, 13.f, 12.f, 4.f, 0.8f, Hex(0x8A6A3A)); C.Poly({ { 11.f, 5.f }, { 14.f, 2.f }, { 12.5f, 6.5f } }, Hex(0xC8C8C8)); C.Poly({ { 2.f, 14.f }, { 6.f, 12.f }, { 4.f, 10.f } }, N == TEXT("spectral_arrow") ? Hex(0xE8C23A) : Hex(0xF0F0F0)); return; }
		if (N == TEXT("bow")) { for (int32 a = 0; a < 16; ++a) { const float A0 = -PI * 0.5f + a / 16.f * PI, A1 = -PI * 0.5f + (a + 1) / 16.f * PI; C.Line(9.f + FMath::Cos(A0) * 5.f, 8.f + FMath::Sin(A0) * 5.f, 9.f + FMath::Cos(A1) * 5.f, 8.f + FMath::Sin(A1) * 5.f, 0.9f, Hex(0x8A5A2A)); } C.Line(9.f, 3.f, 6.f, 8.f, 0.4f, Hex(0xF0F0F0)); C.Line(9.f, 13.f, 6.f, 8.f, 0.4f, Hex(0xF0F0F0)); return; }
		if (N == TEXT("crossbow")) { C.Line(3.f, 10.f, 13.f, 10.f, 1.2f, Hex(0x6A4A2A)); C.Line(4.f, 6.f, 12.f, 14.f, 0.8f, Hex(0x8A5A2A)); C.Line(3.f, 4.f, 13.f, 4.f, 0.8f, Hex(0xC8C8C8)); C.Line(8.f, 2.f, 8.f, 12.f, 0.6f, Hex(0xD8D8D8)); return; }
		if (N == TEXT("shield")) { C.Poly({ { 3.f, 3.f }, { 13.f, 3.f }, { 13.f, 10.f }, { 8.f, 14.5f }, { 3.f, 10.f } }, Hex(0x8A5A2A)); C.Poly({ { 5.f, 5.f }, { 11.f, 5.f }, { 11.f, 9.5f }, { 8.f, 12.f }, { 5.f, 9.5f } }, Hex(0xC8C8C8)); C.Line(8.f, 5.f, 8.f, 12.f, 0.5f, Hex(0xD03020)); return; }
		if (N == TEXT("trident")) { C.Line(6.f, 14.f, 11.f, 6.f, 1.1f, Hex(0x4A9A8A)); C.Poly({ { 9.f, 7.f }, { 8.f, 2.f }, { 10.f, 4.f }, { 11.5f, 1.5f }, { 12.5f, 5.f }, { 14.5f, 4.f }, { 12.f, 8.f } }, Hex(0x5ABAAA)); C.Line(9.5f, 6.f, 12.f, 3.f, 0.4f, Hex(0x9AE0D8)); return; }
		if (N == TEXT("mace")) { C.Line(4.f, 14.f, 10.f, 7.f, 1.4f, Hex(0x6A5A4A)); C.Disc(11.f, 6.f, 4.f, Hex(0x4A4A50)); for (int32 k = 0; k < 6; ++k) { const float A = k / 6.f * 2 * PI; C.Line(11.f + FMath::Cos(A) * 2.f, 6.f + FMath::Sin(A) * 2.f, 11.f + FMath::Cos(A) * 4.6f, 6.f + FMath::Sin(A) * 4.6f, 0.7f, Hex(0x6A6A72)); } return; }
		if (N == TEXT("brush")) { C.Line(4.f, 14.f, 8.f, 9.f, 1.2f, Hex(0x8A6A3A)); C.Poly({ { 6.f, 10.f }, { 11.f, 5.f }, { 14.f, 8.f }, { 9.f, 13.f } }, Hex(0xE8D8B0)); for (int32 k = 0; k < 4; ++k) C.Line(10.5f + k * 0.4f, 5.5f + k * 0.4f, 13.5f + k * 0.4f, 8.5f + k * 0.4f, 0.3f, Hex(0xC8B890)); return; }
		if (N == TEXT("fishing_rod")) { C.Line(3.f, 13.f, 12.f, 3.f, 0.9f, Hex(0x8A6A3A)); for (int32 a = 0; a < 12; ++a) { const float T = a / 12.f; C.Line(12.f + T * 2.f, 3.f + T * 6.f, 12.f + (T + 0.083f) * 2.f, 3.f + (T + 0.083f) * 6.f, 0.3f, Hex(0xE8E8E8)); } C.Disc(13.5f, 9.5f, 1.1f, Hex(0xC8C8C8)); return; }
		if (N == TEXT("elytra")) { C.Poly({ { 8.f, 6.f }, { 3.f, 3.f }, { 2.f, 9.f }, { 8.f, 12.f } }, Hex(0x8A8A9A)); C.Poly({ { 8.f, 6.f }, { 13.f, 3.f }, { 14.f, 9.f }, { 8.f, 12.f } }, Hex(0x9A9AAA)); C.Line(8.f, 6.f, 8.f, 12.f, 0.5f, Hex(0x5A5A6A)); return; }
		if (N == TEXT("glass_bottle")) { C.FillRect(7.f, 1.5f, 9.f, 4.f, Hex(0xC8C8C8)); C.Poly({ { 6.f, 4.f }, { 10.f, 4.f }, { 11.5f, 9.f }, { 11.5f, 13.5f }, { 4.5f, 13.5f }, { 4.5f, 9.f } }, Hex(0xD8E8F0, 0.5f)); return; }
		if (N.Contains(TEXT("boat")) || N.Contains(TEXT("raft"))) { C.Poly({ { 2.f, 8.f }, { 14.f, 8.f }, { 11.5f, 13.f }, { 4.5f, 13.f } }, Hex(0x9A7446)); C.Line(2.5f, 9.f, 13.5f, 9.f, 0.5f, Hex(0x6E5230)); C.Line(4.f, 10.f, 12.f, 10.f, 0.5f, Hex(0x6E5230)); C.Line(5.f, 11.f, 11.f, 11.f, 0.5f, Hex(0x6E5230)); if (N.Contains(TEXT("chest"))) C.FillRect(5.f, 5.f, 11.f, 8.f, Hex(0x8A5A2A)); return; }
		if (N.EndsWith(TEXT("minecart")) || N == TEXT("minecart")) { C.Poly({ { 2.f, 7.f }, { 14.f, 7.f }, { 12.f, 12.f }, { 4.f, 12.f } }, Hex(0x9A9AA2)); C.Line(2.f, 7.f, 14.f, 7.f, 1.f, Hex(0xC8C8D0)); C.Disc(4.5f, 13.f, 1.2f, Hex(0x4A4A50)); C.Disc(11.5f, 13.f, 1.2f, Hex(0x4A4A50)); return; }
		if (N == TEXT("bucket")) { }
		if (N == TEXT("armor_stand")) { C.Line(8.f, 3.f, 8.f, 12.f, 0.8f, Hex(0x9A7446)); C.Line(4.f, 6.f, 12.f, 6.f, 0.7f, Hex(0x9A7446)); C.Line(5.f, 12.f, 11.f, 12.f, 0.6f, Hex(0x9A7446)); C.Line(8.f, 12.f, 8.f, 14.f, 0.8f, Hex(0x9A7446)); C.Disc(8.f, 2.f, 1.2f, Hex(0xC8A878)); return; }
		if (N == TEXT("item_frame") || N == TEXT("painting")) { C.FillRect(2.f, 2.f, 14.f, 14.f, N == TEXT("painting") ? Hex(0x6A4A2A) : Hex(0x9A7446)); C.FillRect(3.5f, 3.5f, 12.5f, 12.5f, N == TEXT("painting") ? Hex(0x4A8AC8) : Hex(0xD8C8A8)); if (N == TEXT("painting")) { C.Poly({ { 4.f, 12.f }, { 8.f, 6.f }, { 12.f, 12.f } }, Hex(0x4A8A3A)); C.Disc(10.f, 6.f, 1.2f, Hex(0xF0D040)); } return; }
		if (N == TEXT("goat_horn")) { for (int32 a = 0; a < 14; ++a) { const float T = a / 14.f; const float X = FMath::Lerp(2.f, 13.f, T), Y = FMath::Lerp(13.f, 5.f, T) - FMath::Sin(T * PI) * 1.5f; C.Disc(X, Y, FMath::Lerp(1.4f, 0.7f, T), Hex(0xE8DCA8 + (uint32)(T * 0x0A0A0A))); } return; }
		if (N == TEXT("bundle")) { C.Poly({ { 4.f, 6.f }, { 12.f, 6.f }, { 13.f, 14.f }, { 3.f, 14.f } }, Hex(0xB07A4A)); C.Line(3.f, 8.f, 13.f, 8.f, 0.6f, Hex(0x7A4E28)); C.Line(6.f, 4.f, 10.f, 4.f, 0.6f, Hex(0x7A4E28)); return; }
		if (N.Contains(TEXT("_dye"))) { const uint32 H = Seed; const FLinearColor M = Hex(0xE9ECEC); C.Poly({ { 4.f, 8.f }, { 12.f, 8.f }, { 14.f, 11.f }, { 12.f, 14.f }, { 4.f, 14.f }, { 2.f, 11.f } }, Hex(0xC8C8C8)); C.Poly({ { 5.5f, 9.5f }, { 10.5f, 9.5f }, { 12.f, 11.f }, { 10.5f, 12.5f }, { 5.5f, 12.5f }, { 4.f, 11.f } }, Hex(0x1A1A1A)); (void)M; (void)H; return; }
		if (N.Contains(TEXT("_sign"))) { C.FillRect(2.f, 3.f, 14.f, 10.f, Hex(0xB8945F)); C.Line(4.f, 5.f, 12.f, 5.f, 0.4f, Hex(0x6B5335)); C.Line(4.f, 7.f, 11.f, 7.f, 0.4f, Hex(0x6B5335)); C.Line(8.f, 10.f, 8.f, 14.f, 0.8f, Hex(0x8A6B3E)); return; }
		if (N.Contains(TEXT("_door"))) { C.FillRect(4.f, 2.f, 12.f, 15.f, Hex(0xB8945F)); C.FillRect(5.f, 4.f, 11.f, 8.f, Hex(0x96744A)); C.FillRect(5.f, 9.f, 11.f, 13.f, Hex(0x96744A)); return; }
		if (N.Contains(TEXT("_trapdoor"))) { C.FillRect(2.f, 4.f, 14.f, 12.f, Hex(0xB8945F)); C.Line(8.f, 4.f, 8.f, 12.f, 0.5f, Hex(0x6B5335)); C.Line(2.f, 8.f, 14.f, 8.f, 0.5f, Hex(0x6B5335)); return; }
		// fallback: a small crate with the item's initial
		C.FillRect(3.f, 6.f, 13.f, 13.f, Hex(0x8A8A8A));
		C.FillRect(4.f, 7.f, 12.f, 12.f, Hex(0xB0B0B0));
	}
}
