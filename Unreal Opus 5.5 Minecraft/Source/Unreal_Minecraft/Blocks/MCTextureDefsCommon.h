// Small DSL helpers for writing texture definition tables.
#pragma once

#include "Blocks/MCTextures.h"

namespace MCTexDSL
{
	FORCEINLINE FColor Hex(uint32 H) { return FColor((H >> 16) & 255, (H >> 8) & 255, H & 255, 255); }

	FORCEINLINE FMCTexDef Def(const TCHAR* Name, EMCTexRecipe R, uint32 A, uint32 B, uint32 C, float P0, float P1, float P2, const TCHAR* Base, uint8 Flags)
	{
		FMCTexDef D;
		D.Name = FName(Name);
		D.Recipe = R;
		D.C0 = Hex(A);
		D.C1 = Hex(B);
		D.C2 = Hex(C);
		D.P0 = P0; D.P1 = P1; D.P2 = P2;
		if (Base) D.Base = FName(Base);
		D.Flags = Flags;
		return D;
	}

	/** The 16 dye colours (Minecraft order). */
	struct FDyeColor { const TCHAR* Name; uint32 Rgb; };
	static const FDyeColor DyeColors[16] = {
		{ TEXT("white"), 0xE9ECEC }, { TEXT("orange"), 0xF07613 }, { TEXT("magenta"), 0xBD44B3 }, { TEXT("light_blue"), 0x3AAFD9 },
		{ TEXT("yellow"), 0xF8C627 }, { TEXT("lime"), 0x70B919 }, { TEXT("pink"), 0xED8DAC }, { TEXT("gray"), 0x3E4447 },
		{ TEXT("light_gray"), 0x8E8E86 }, { TEXT("cyan"), 0x158991 }, { TEXT("purple"), 0x792AAC }, { TEXT("blue"), 0x35399D },
		{ TEXT("brown"), 0x724728 }, { TEXT("green"), 0x546D1B }, { TEXT("red"), 0xA12722 }, { TEXT("black"), 0x141519 }
	};

	/** Terracotta has its own muted palette. */
	static const uint32 TerracottaColors[16] = {
		0xD1B2A1, 0xA1531F, 0x95576C, 0x706C8A, 0xBA8523, 0x677534, 0xA14E4E, 0x392A23,
		0x876A61, 0x575B5B, 0x764656, 0x4A3B5B, 0x4D3323, 0x4C532A, 0x8F3D2E, 0x251610
	};

	struct FWoodPalette
	{
		const TCHAR* Name;
		uint32 Planks, PlanksGrain, Bark, BarkDark, Top, TopRing, Stripped, StrippedDark, Leaves;
		bool bTintedLeaves;
		bool bNether;
	};

	static const FWoodPalette Woods[12] = {
		{ TEXT("oak"),      0xB8945F, 0x96744A, 0x6B5335, 0x3E301E, 0xB08E5A, 0x8A6B3E, 0xB18D56, 0x93713F, 0x9A9A9A, true, false },
		{ TEXT("spruce"),   0x7A5A36, 0x5E452A, 0x3E2D1C, 0x241A10, 0x7C5B38, 0x5E4428, 0x8C6A42, 0x6E5232, 0x8A8A8A, true, false },
		{ TEXT("birch"),    0xD7C68E, 0xBFAE78, 0xE3E0D6, 0x2A2A28, 0xD4C791, 0xB8A874, 0xC8B67C, 0xAE9C64, 0xA0A0A0, true, false },
		{ TEXT("jungle"),   0xB5835E, 0x996B49, 0x5A4A22, 0x3E3316, 0xA57A4E, 0x86603A, 0xAE7E50, 0x92663E, 0x9C9C9C, true, false },
		{ TEXT("acacia"),   0xB55E36, 0x9A4C2A, 0x6E665B, 0x4F4940, 0xA45A34, 0x843E22, 0xAE5C38, 0x8E4628, 0x969696, true, false },
		{ TEXT("dark_oak"), 0x4E341B, 0x3D2814, 0x3B2A18, 0x21170C, 0x4A3219, 0x362412, 0x5A3E24, 0x442E18, 0x8E8E8E, true, false },
		{ TEXT("mangrove"), 0x7A3530, 0x632A26, 0x56442F, 0x3A2E20, 0x6E3A2E, 0x552A22, 0x8A3E36, 0x6E302A, 0x929292, true, false },
		{ TEXT("cherry"),   0xE4B5A8, 0xCF9A8D, 0x3A2230, 0x241420, 0xD9A597, 0xB88272, 0xD6A090, 0xBC8676, 0xF0B6CC, false, false },
		{ TEXT("pale_oak"), 0xE8DED5, 0xD2C6BA, 0x6B645E, 0x4A4540, 0xE0D6CB, 0xC4B8AA, 0xE2D8CE, 0xC8BCB0, 0xB8BBAE, false, false },
		{ TEXT("bamboo"),   0xC9B45A, 0xB19D45, 0x7E8F32, 0x5A6A20, 0xC0AE56, 0x9A8A3E, 0xC4B052, 0xA8963E, 0x8CA040, false, false },
		{ TEXT("crimson"),  0x6B3048, 0x562539, 0x5C1D2E, 0x3E1320, 0x6A2A3E, 0x8E2E3E, 0x8A3A52, 0x6E2A40, 0x7A0B0E, false, true },
		{ TEXT("warped"),   0x2B6963, 0x215450, 0x2A4A4F, 0x1B3033, 0x2E6B64, 0x3AB7A6, 0x3A8A80, 0x2A6A62, 0x167C84, false, true }
	};
}
