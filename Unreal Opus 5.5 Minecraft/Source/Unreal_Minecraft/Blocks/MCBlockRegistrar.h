// Fluent builder used by the block registration tables.
#pragma once

#include "CoreMinimal.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCBlockBehaviors.h"
#include "Blocks/MCTextures.h"

class FMCBlockBuilder
{
public:
	explicit FMCBlockBuilder(FMCBlock& InB) : B(InB) {}
	FMCBlock& B;

	FMCBlockBuilder& Tex(const TCHAR* All) { for (int32 i = 0; i < 6; ++i) B.TexNames[i] = FName(All); return *this; }
	/** Top, bottom, side */
	FMCBlockBuilder& TexTBS(const TCHAR* Top, const TCHAR* Bottom, const TCHAR* Side)
	{
		for (int32 i = 2; i < 6; ++i) B.TexNames[i] = FName(Side);
		B.TexNames[(int32)EMCFace::Up] = FName(Top);
		B.TexNames[(int32)EMCFace::Down] = FName(Bottom);
		return *this;
	}
	FMCBlockBuilder& TexTS(const TCHAR* TopBottom, const TCHAR* Side) { return TexTBS(TopBottom, TopBottom, Side); }
	/** Front/back/side/top/bottom for oriented cubes (front stored in North, back in South). */
	FMCBlockBuilder& TexFBSTB(const TCHAR* Front, const TCHAR* Back, const TCHAR* Side, const TCHAR* Top, const TCHAR* Bottom)
	{
		B.TexNames[(int32)EMCFace::North] = FName(Front);
		B.TexNames[(int32)EMCFace::South] = FName(Back);
		B.TexNames[(int32)EMCFace::West] = FName(Side);
		B.TexNames[(int32)EMCFace::East] = FName(Side);
		B.TexNames[(int32)EMCFace::Up] = FName(Top);
		B.TexNames[(int32)EMCFace::Down] = FName(Bottom);
		return *this;
	}
	FMCBlockBuilder& TexFace(EMCFace F, const TCHAR* Name) { B.TexNames[(int32)F] = FName(Name); return *this; }
	FMCBlockBuilder& Alt(int32 I, const TCHAR* Name) { B.TexAlt[I] = FMCTextures::Find(FName(Name)); return *this; }
	FMCBlockBuilder& Hard(float H, float Blast = -1.f) { B.Hardness = H; B.BlastResistance = Blast < 0 ? H : Blast; if (H < 0) { B.Flags |= MCB_Unbreakable; } return *this; }
	FMCBlockBuilder& Tool(EMCTool T, uint8 Tier = 0, bool bRequired = true) { B.Tool = T; B.MinTier = Tier; B.bRequiresTool = bRequired; return *this; }
	FMCBlockBuilder& Pick(uint8 Tier = MCTier::Wood) { return Tool(EMCTool::Pickaxe, Tier, true); }
	FMCBlockBuilder& Axe() { return Tool(EMCTool::Axe, 0, false); }
	FMCBlockBuilder& Shovel() { return Tool(EMCTool::Shovel, 0, false); }
	FMCBlockBuilder& Hoe() { return Tool(EMCTool::Hoe, 0, false); }
	FMCBlockBuilder& Snd(EMCSound S) { B.Sound = S; return *this; }
	FMCBlockBuilder& Tab(EMCTab T) { B.Tab = T; return *this; }
	FMCBlockBuilder& Light(uint8 L) { B.LightEmission = L; return *this; }
	FMCBlockBuilder& Opacity(uint8 O) { B.LightOpacity = O; return *this; }
	FMCBlockBuilder& Flag(uint32 F) { B.Flags |= F; return *this; }
	FMCBlockBuilder& NoFlag(uint32 F) { B.Flags &= ~F; return *this; }
	FMCBlockBuilder& Meta(uint8 Bits) { B.MetaBits = Bits; return *this; }
	FMCBlockBuilder& Orient(EMCCubeOrient O)
	{
		B.Orient = O;
		switch (O)
		{
		case EMCCubeOrient::Axis: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 2); break;
		case EMCCubeOrient::Facing4: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 2); break;
		case EMCCubeOrient::Facing4Lit: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 3); break;
		case EMCCubeOrient::Facing6: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 3); break;
		case EMCCubeOrient::Facing6Lit: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 4); break;
		case EMCCubeOrient::LitToggle: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 1); break;
		case EMCCubeOrient::Upper: B.MetaBits = FMath::Max<uint8>(B.MetaBits, 1); break;
		default: break;
		}
		return *this;
	}
	/** Transparent, non-occluding cube variants. */
	FMCBlockBuilder& Cutout() { B.Layer = EMCLayer::Cutout; B.Flags &= ~MCB_Opaque; B.Flags |= MCB_Cutout; B.LightOpacity = FMath::Min<uint8>(B.LightOpacity, 1); return *this; }
	FMCBlockBuilder& Translucent() { B.Layer = EMCLayer::Translucent; B.Flags &= ~MCB_Opaque; B.LightOpacity = FMath::Min<uint8>(B.LightOpacity, 1); return *this; }
	FMCBlockBuilder& Shape(EMCShape S)
	{
		B.Shape = S;
		if (S != EMCShape::Cube) { B.Flags &= ~MCB_Opaque; B.LightOpacity = FMath::Min<uint8>(B.LightOpacity, 1); }
		if (S == EMCShape::Cross || S == EMCShape::Crop) { B.Layer = EMCLayer::Cutout; B.Flags &= ~MCB_Solid; B.LightOpacity = 0; }
		if (S == EMCShape::Air) { B.Flags = MCB_Replaceable | MCB_NoItem; B.LightOpacity = 0; }
		return *this;
	}
	FMCBlockBuilder& Model(EMCModel M, uint8 MetaBits = 0)
	{
		B.Shape = EMCShape::Model; B.Model = M; B.MetaBits = FMath::Max<uint8>(B.MetaBits, MetaBits);
		B.Flags &= ~MCB_Opaque; B.LightOpacity = 0;
		return *this;
	}
	FMCBlockBuilder& NoCollision() { B.Flags &= ~MCB_Solid; return *this; }
	FMCBlockBuilder& Replaceable() { B.Flags |= MCB_Replaceable; return *this; }
	FMCBlockBuilder& Tint(EMCTint T, FColor C = FColor::White) { B.Tint = T; B.TintColor = C; return *this; }
	FMCBlockBuilder& DropNone() { B.Drop.Type = EMCDropType::None; return *this; }
	FMCBlockBuilder& DropSilk() { B.Drop.Type = EMCDropType::SilkOnly; return *this; }
	FMCBlockBuilder& DropItem(const TCHAR* Item, uint8 Min = 1, uint8 Max = 1, bool bFortune = false)
	{
		B.Drop.Type = EMCDropType::Item; B.Drop.Item = FName(Item); B.Drop.Min = Min; B.Drop.Max = Max; B.Drop.bFortune = bFortune; return *this;
	}
	FMCBlockBuilder& DropSpecial() { B.Drop.Type = EMCDropType::Special; return *this; }
	FMCBlockBuilder& NoSilk() { B.Drop.bSilkSelf = false; return *this; }
	FMCBlockBuilder& Beh(EMCBeh Id) { B.Behavior = MCBehaviors::Get(Id); return *this; }
	FMCBlockBuilder& Friction(float F) { B.Friction = F; return *this; }
	FMCBlockBuilder& Speed(float S) { B.SpeedFactor = S; return *this; }
	FMCBlockBuilder& Jump(float J) { B.JumpFactor = J; return *this; }
	FMCBlockBuilder& Fuel(int32 Ticks) { B.FuelTicks = Ticks; return *this; }
	FMCBlockBuilder& Burn(uint8 Spread, uint8 Flammability) { B.FireSpread = Spread; B.FireBurn = Flammability; B.Flags |= MCB_Flammable; return *this; }
	FMCBlockBuilder& Tag(const TCHAR* T) { B.Tags.AddUnique(FName(T)); return *this; }
	FMCBlockBuilder& Fam(const TCHAR* Family, const TCHAR* Variant) { B.Family = FName(Family); B.Variant = FName(Variant); return *this; }
	FMCBlockBuilder& Mesh(const TCHAR* Asset) { B.MeshAsset = FName(Asset); B.Flags |= MCB_AuthoredMesh; return *this; }
	FMCBlockBuilder& XP(float Min, float Max) { B.XPMin = Min; B.XPMax = Max; return *this; }
	FMCBlockBuilder& Item(const TCHAR* Name) { B.ItemName = FName(Name); return *this; }
	FMCBlockBuilder& NoItem() { B.Flags |= MCB_NoItem; return *this; }
	FMCBlockBuilder& Map(uint32 Hex) { B.MapColor = FColor((Hex >> 16) & 255, (Hex >> 8) & 255, Hex & 255); return *this; }
	FMCBlockBuilder& RandomRotate() { B.bRandomRotateTop = true; return *this; }
	FMCBlockBuilder& Ticks() { B.Flags |= MCB_RandomTick; return *this; }
};

class FMCBlockRegistrar
{
public:
	explicit FMCBlockRegistrar(TArray<FMCBlock>& InBlocks) : Blocks(InBlocks) {}
	TArray<FMCBlock>& Blocks;

	FMCBlockBuilder Add(const TCHAR* Name, const TCHAR* Display = nullptr);
	FMCBlock* Find(const TCHAR* Name);

	// ---- family helpers (derive from an existing base block)
	FMCBlockBuilder Slab(const TCHAR* Base, const TCHAR* Name = nullptr);
	FMCBlockBuilder Stairs(const TCHAR* Base, const TCHAR* Name = nullptr);
	FMCBlockBuilder Wall(const TCHAR* Base, const TCHAR* Name = nullptr);
	FMCBlockBuilder Fence(const TCHAR* Base, const TCHAR* Name, bool bNether = false);
	FMCBlockBuilder FenceGate(const TCHAR* Base, const TCHAR* Name);
	FMCBlockBuilder Button(const TCHAR* Base, const TCHAR* Name, bool bWood);
	FMCBlockBuilder PressurePlate(const TCHAR* Base, const TCHAR* Name, bool bWood);
	/** Stairs + slab (+ wall) for a stone-like block */
	void StoneSet(const TCHAR* Base, bool bWall, const TCHAR* StairsName = nullptr, const TCHAR* SlabName = nullptr, const TCHAR* WallName = nullptr);

	static FString Pretty(const TCHAR* Id);

private:
	FMCBlockBuilder Derive(const TCHAR* Base, const TCHAR* Name, const TCHAR* Suffix);
};
