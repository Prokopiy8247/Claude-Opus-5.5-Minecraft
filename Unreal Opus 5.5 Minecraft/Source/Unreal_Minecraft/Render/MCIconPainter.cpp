// Procedural item sprites, part 1: tools, weapons, armour, materials and minerals.
#include "Render/MCIcons.h"
#include "Render/MCIconCanvas.h"

using namespace MCIconDraw;

namespace MCIconPaint2
{
	bool PaintMisc(const FMCItem& I, const FString& N, FIco& C); // MCIconPainter2.cpp
}

namespace
{
	struct FMat { FLinearColor Base, Light, Dark; };

	FMat MaterialOf(const FString& N)
	{
		auto M = [](uint32 B, uint32 L, uint32 D) { return FMat{ Hex(B), Hex(L), Hex(D) }; };
		if (N.StartsWith(TEXT("wooden"))) return M(0x9A7446, 0xB89060, 0x5E4424);
		if (N.StartsWith(TEXT("stone"))) return M(0x8A8A8A, 0xB0B0B0, 0x55555A);
		if (N.StartsWith(TEXT("copper"))) return M(0xC8754A, 0xE8A07A, 0x7E4228);
		if (N.StartsWith(TEXT("iron"))) return M(0xD6D6D6, 0xF4F4F4, 0x8A8A8A);
		if (N.StartsWith(TEXT("golden")) || N.StartsWith(TEXT("gold"))) return M(0xF2D34A, 0xFFF0A0, 0xB08A1E);
		if (N.StartsWith(TEXT("diamond"))) return M(0x4EDCD2, 0xB8FFF8, 0x1E8A84);
		if (N.StartsWith(TEXT("netherite"))) return M(0x4A4046, 0x6E6268, 0x241E22);
		if (N.StartsWith(TEXT("leather"))) return M(0x9A5A34, 0xC07A4E, 0x5E3418);
		if (N.StartsWith(TEXT("chainmail"))) return M(0x9A9AA2, 0xC8C8D0, 0x4E4E56);
		if (N.StartsWith(TEXT("turtle"))) return M(0x4A9A3A, 0x7AC85A, 0x2A5A1E);
		return M(0x9A9A9A, 0xC8C8C8, 0x5A5A5A);
	}

	const FLinearColor Wood = Hex(0x8A6438), WoodDark = Hex(0x4E361C);

	void Handle(FIco& C, float X0, float Y0, float X1, float Y1)
	{
		C.Line(X0, Y0, X1, Y1, 1.4f, WoodDark);
		C.Line(X0, Y0 - 0.25f, X1, Y1 - 0.25f, 0.8f, Wood);
	}

	void PaintTool(FIco& C, const FString& N, const FMat& M)
	{
		if (N.EndsWith(TEXT("_sword")))
		{
			Handle(C, 2.5f, 13.5f, 5.5f, 10.5f);
			C.Poly({ { 5.f, 9.5f }, { 12.5f, 2.f }, { 14.5f, 1.5f }, { 14.f, 3.5f }, { 6.5f, 11.f } }, M.Base);
			C.Line(6.f, 10.f, 13.5f, 2.5f, 0.5f, M.Light);
			C.Line(3.f, 8.f, 8.f, 13.f, 1.2f, M.Dark); // guard
			C.Disc(2.f, 14.f, 1.1f, M.Dark);
		}
		else if (N.EndsWith(TEXT("_pickaxe")))
		{
			Handle(C, 2.5f, 14.f, 11.f, 5.5f);
			C.Poly({ { 3.5f, 4.5f }, { 7.f, 1.8f }, { 11.f, 1.5f }, { 14.5f, 5.f }, { 14.2f, 9.f }, { 12.8f, 12.5f }, { 11.5f, 8.f }, { 9.f, 5.2f }, { 6.5f, 4.8f } }, M.Base);
			C.Line(5.f, 3.8f, 10.5f, 2.3f, 0.5f, M.Light);
			C.Line(12.f, 3.5f, 13.6f, 8.f, 0.5f, M.Light);
		}
		else if (N.EndsWith(TEXT("_axe")))
		{
			Handle(C, 3.f, 14.f, 11.5f, 4.f);
			C.Poly({ { 8.f, 2.f }, { 11.5f, 0.8f }, { 14.8f, 2.5f }, { 15.f, 7.f }, { 12.5f, 8.5f }, { 10.f, 6.f } }, M.Base);
			C.Line(11.5f, 1.4f, 14.3f, 3.f, 0.5f, M.Light);
		}
		else if (N.EndsWith(TEXT("_shovel")))
		{
			Handle(C, 2.5f, 14.f, 9.5f, 6.5f);
			C.Ellipse(11.8f, 4.2f, 3.4f, 2.3f, -PI * 0.25f, M.Base);
			C.Line(10.5f, 3.f, 13.f, 1.8f, 0.5f, M.Light);
		}
		else if (N.EndsWith(TEXT("_hoe")))
		{
			Handle(C, 2.5f, 14.f, 11.f, 4.5f);
			C.Poly({ { 8.5f, 2.f }, { 14.f, 2.f }, { 14.f, 4.f }, { 12.f, 4.f }, { 12.f, 6.f }, { 10.f, 6.f }, { 10.f, 4.f }, { 8.5f, 4.f } }, M.Base);
			C.Line(9.f, 2.5f, 13.5f, 2.5f, 0.5f, M.Light);
		}
		else if (N.EndsWith(TEXT("_spear")))
		{
			C.Line(1.5f, 14.5f, 11.f, 5.f, 1.1f, WoodDark);
			C.Line(1.5f, 14.2f, 11.f, 4.7f, 0.6f, Wood);
			C.Poly({ { 10.f, 6.f }, { 12.f, 2.5f }, { 15.f, 1.f }, { 13.5f, 4.f }, { 10.5f, 6.5f } }, M.Base);
			C.Line(11.f, 5.f, 14.f, 1.8f, 0.45f, M.Light);
			C.FillRect(9.f, 6.f, 10.5f, 7.5f, M.Dark);
		}
	}

	void PaintArmor(FIco& C, const FString& N, const FMat& M, bool bChain)
	{
		const FLinearColor B = M.Base, L = M.Light, D = M.Dark;
		if (N.EndsWith(TEXT("_helmet")))
		{
			C.Poly({ { 2.5f, 12.f }, { 2.5f, 6.f }, { 5.f, 3.f }, { 11.f, 3.f }, { 13.5f, 6.f }, { 13.5f, 12.f }, { 11.f, 12.f }, { 11.f, 8.5f }, { 5.f, 8.5f }, { 5.f, 12.f } }, B);
			C.FillRect(4.f, 4.f, 12.f, 5.f, L);
		}
		else if (N.EndsWith(TEXT("_chestplate")))
		{
			C.Poly({ { 1.5f, 3.f }, { 5.5f, 2.f }, { 6.5f, 4.f }, { 9.5f, 4.f }, { 10.5f, 2.f }, { 14.5f, 3.f }, { 14.5f, 8.f }, { 12.f, 8.f }, { 12.f, 14.f }, { 4.f, 14.f }, { 4.f, 8.f }, { 1.5f, 8.f } }, B);
			C.FillRect(5.f, 6.f, 11.f, 7.f, L);
			C.Line(8.f, 5.f, 8.f, 13.f, 0.4f, D);
		}
		else if (N.EndsWith(TEXT("_leggings")))
		{
			C.Poly({ { 3.f, 2.f }, { 13.f, 2.f }, { 13.f, 14.f }, { 10.f, 14.f }, { 8.5f, 6.f }, { 7.5f, 6.f }, { 6.f, 14.f }, { 3.f, 14.f } }, B);
			C.FillRect(3.f, 2.f, 13.f, 3.5f, D);
			C.FillRect(3.5f, 4.f, 5.f, 13.f, L);
		}
		else if (N.EndsWith(TEXT("_boots")))
		{
			C.Poly({ { 2.f, 6.f }, { 6.f, 6.f }, { 6.f, 11.f }, { 7.f, 13.5f }, { 1.f, 13.5f }, { 2.f, 11.f } }, B);
			C.Poly({ { 10.f, 6.f }, { 14.f, 6.f }, { 14.f, 11.f }, { 15.f, 13.5f }, { 9.f, 13.5f }, { 10.f, 11.f } }, B);
			C.FillRect(2.f, 6.f, 6.f, 7.f, L); C.FillRect(10.f, 6.f, 14.f, 7.f, L);
		}
		if (bChain)
			for (int32 y = 0; y < FIco::S; ++y) for (int32 x = 0; x < FIco::S; ++x) if (((x + y) % 4 == 0) && C.At(x, y).A > 0.5f) C.At(x, y) = FLinearColor(0, 0, 0, 0);
	}

	void Ingot(FIco& C, const FMat& M)
	{
		C.Poly({ { 1.5f, 10.f }, { 5.f, 6.f }, { 14.5f, 6.f }, { 11.f, 10.f } }, M.Light);
		C.Poly({ { 1.5f, 10.f }, { 11.f, 10.f }, { 11.f, 12.5f }, { 1.5f, 12.5f } }, M.Base);
		C.Poly({ { 11.f, 10.f }, { 14.5f, 6.f }, { 14.5f, 8.5f }, { 11.f, 12.5f } }, M.Dark);
	}

	void Nugget(FIco& C, const FMat& M)
	{
		C.Disc(6.f, 9.f, 2.4f, M.Base); C.Disc(9.5f, 7.f, 2.1f, M.Base); C.Disc(10.f, 10.5f, 1.8f, M.Dark);
		C.Disc(5.5f, 8.2f, 0.9f, M.Light); C.Disc(9.f, 6.3f, 0.8f, M.Light);
	}

	void Lump(FIco& C, const FLinearColor& A, const FLinearColor& B, uint32 Seed)
	{
		C.Poly({ { 3.f, 8.f }, { 5.5f, 3.5f }, { 10.f, 3.f }, { 13.f, 6.f }, { 13.5f, 10.5f }, { 10.f, 13.5f }, { 5.f, 13.f } }, A);
		for (int32 k = 0; k < 6; ++k) { const float X = 4.5f + (float)((Seed * (k + 3)) % 7), Y = 5.f + (float)((Seed * (k + 5)) % 6); C.Disc(X, Y, 0.9f, B); }
	}

	void Gem(FIco& C, const FLinearColor& A, const FLinearColor& L, const FLinearColor& D, int32 Cut)
	{
		if (Cut == 0) // brilliant
		{
			C.Poly({ { 2.f, 6.f }, { 5.f, 2.5f }, { 11.f, 2.5f }, { 14.f, 6.f }, { 8.f, 14.f } }, A);
			C.Poly({ { 2.f, 6.f }, { 14.f, 6.f }, { 8.f, 14.f } }, Sh(A, 0.8f));
			C.Poly({ { 5.f, 2.5f }, { 8.f, 6.f }, { 11.f, 2.5f } }, L);
			C.Line(8.f, 6.f, 8.f, 13.f, 0.4f, D);
		}
		else if (Cut == 1) // rhombus
		{
			C.Poly({ { 8.f, 1.5f }, { 13.f, 8.f }, { 8.f, 14.5f }, { 3.f, 8.f } }, A);
			C.Poly({ { 8.f, 1.5f }, { 13.f, 8.f }, { 8.f, 8.f } }, L);
			C.Poly({ { 3.f, 8.f }, { 8.f, 14.5f }, { 8.f, 8.f } }, D);
		}
		else // shard
		{
			C.Poly({ { 5.f, 14.f }, { 3.5f, 7.f }, { 8.f, 1.5f }, { 12.5f, 6.f }, { 10.5f, 14.f } }, A);
			C.Line(8.f, 2.5f, 7.f, 13.f, 0.6f, L);
		}
	}

	void Dust(FIco& C, const FLinearColor& A, const FLinearColor& B, uint32 Seed)
	{
		C.Ellipse(8.f, 11.f, 5.5f, 2.6f, 0.f, Sh(A, 0.8f));
		C.Ellipse(8.f, 9.5f, 4.f, 3.f, 0.f, A);
		for (int32 k = 0; k < 9; ++k) C.Disc(4.f + (float)((Seed * (k + 7)) % 9), 6.5f + (float)((Seed * (k + 11)) % 6), 0.55f, (k & 1) ? B : Sh(A, 1.2f));
	}

	void Rod(FIco& C, const FLinearColor& A, const FLinearColor& L, float Thick)
	{
		C.Line(3.f, 13.f, 13.f, 3.f, Thick, A);
		C.Line(3.f, 12.6f, 13.f, 2.6f, Thick * 0.45f, L);
	}
}

namespace MCIcons
{
	bool PaintItem(const FMCItem& I, FColor* Out)
	{
		FIco C;
		const FString N = I.Name.ToString();
		bool bDone = true;
		const bool bTool = N.EndsWith(TEXT("_sword")) || N.EndsWith(TEXT("_pickaxe")) || N.EndsWith(TEXT("_axe")) || N.EndsWith(TEXT("_shovel")) || N.EndsWith(TEXT("_hoe")) || N.EndsWith(TEXT("_spear"));
		if (bTool && I.Kind != EMCItemKind::Block) PaintTool(C, N, MaterialOf(N));
		else if (I.Kind == EMCItemKind::Armor && !N.Contains(TEXT("horse")) && N != TEXT("wolf_armor")) PaintArmor(C, N, MaterialOf(N), N.StartsWith(TEXT("chainmail")));
		else if (N.EndsWith(TEXT("_ingot"))) Ingot(C, MaterialOf(N.Replace(TEXT("_ingot"), TEXT(""))));
		else if (N.EndsWith(TEXT("_nugget"))) Nugget(C, MaterialOf(N.Replace(TEXT("_nugget"), TEXT(""))));
		else if (N == TEXT("raw_iron")) Lump(C, Hex(0xB08A70), Hex(0xE0C0A8), 3);
		else if (N == TEXT("raw_copper")) Lump(C, Hex(0xB06A40), Hex(0x6FB09A), 5);
		else if (N == TEXT("raw_gold")) Lump(C, Hex(0xD8A82A), Hex(0xFFE070), 7);
		else if (N == TEXT("coal")) Lump(C, Hex(0x2A2A2A), Hex(0x505050), 11);
		else if (N == TEXT("charcoal")) Lump(C, Hex(0x3A3028), Hex(0x5A4C40), 13);
		else if (N == TEXT("netherite_scrap")) Lump(C, Hex(0x5A4238), Hex(0x8A6A5E), 17);
		else if (N == TEXT("diamond")) Gem(C, Hex(0x5EE2DA), Hex(0xC8FFFA), Hex(0x2A9E98), 0);
		else if (N == TEXT("emerald")) Gem(C, Hex(0x2AC85A), Hex(0x9AF0B0), Hex(0x0E7A34), 1);
		else if (N == TEXT("lapis_lazuli")) Gem(C, Hex(0x2A58C8), Hex(0x6A90F0), Hex(0x142E7A), 1);
		else if (N == TEXT("quartz")) Gem(C, Hex(0xEEE8E0), Hex(0xFFFFFF), Hex(0xB8B0A4), 2);
		else if (N == TEXT("amethyst_shard")) Gem(C, Hex(0x9A6AD8), Hex(0xD8B8FF), Hex(0x5A3A8E), 2);
		else if (N == TEXT("echo_shard")) Gem(C, Hex(0x1A4A5A), Hex(0x3AE0F0), Hex(0x0A2A34), 2);
		else if (N == TEXT("prismarine_shard")) Gem(C, Hex(0x5AA898), Hex(0x9AE0D0), Hex(0x2E6A5E), 2);
		else if (N == TEXT("sulfur_shard")) Gem(C, Hex(0xD8C040), Hex(0xFFF090), Hex(0x8E7A1E), 2);
		else if (N == TEXT("redstone")) Dust(C, Hex(0xB01010), Hex(0xFF5040), 3);
		else if (N == TEXT("glowstone_dust")) Dust(C, Hex(0xD8A838), Hex(0xFFF0A0), 5);
		else if (N == TEXT("sugar")) Dust(C, Hex(0xF0F0F0), Hex(0xFFFFFF), 7);
		else if (N == TEXT("gunpowder")) Dust(C, Hex(0x5A5A5A), Hex(0x8A8A8A), 9);
		else if (N == TEXT("blaze_powder")) Dust(C, Hex(0xE8901A), Hex(0xFFE060), 11);
		else if (N == TEXT("bone_meal")) Dust(C, Hex(0xE8E6DA), Hex(0xFFFFFF), 13);
		else if (N == TEXT("cinnabar_dust")) Dust(C, Hex(0xB0302A), Hex(0xE86050), 15);
		else if (N == TEXT("prismarine_crystals")) Dust(C, Hex(0x9AD8C8), Hex(0xF0FFF8), 17);
		else if (N == TEXT("stick")) Rod(C, Hex(0x7A5A30), Hex(0xA07E4A), 1.1f);
		else if (N == TEXT("blaze_rod")) Rod(C, Hex(0xE8A020), Hex(0xFFF080), 1.2f);
		else if (N == TEXT("breeze_rod")) Rod(C, Hex(0x8AA0E0), Hex(0xE0E8FF), 1.2f);
		else if (N == TEXT("bone")) { Rod(C, Hex(0xE6E2D2), Hex(0xFFFFFF), 1.2f); C.Disc(3.f, 12.f, 1.3f, Hex(0xE6E2D2)); C.Disc(4.f, 13.5f, 1.3f, Hex(0xE6E2D2)); C.Disc(12.f, 2.5f, 1.3f, Hex(0xE6E2D2)); C.Disc(13.5f, 4.f, 1.3f, Hex(0xE6E2D2)); }
		else bDone = MCIconPaint2::PaintMisc(I, N, C);
		if (!bDone) return false;
		C.Finish();
		C.Write(Out);
		return true;
	}
}
