// Icon registry: textured isometric renders of block items, procedural sprites for other items, UI glyphs,
// the UI atlas with Slate brushes and the sprite Texture2DArray used by in-world item models.
#include "Render/MCIcons.h"
#include "Render/MCTextureSynth.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "Gen/MCBiomes.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "TextureResource.h"
#include "Styling/SlateBrush.h"
#include "Async/ParallelFor.h"

namespace
{
	constexpr int32 Cell = MCIcons::CellSize;      // 64
	constexpr int32 Spr = MCIcons::SpriteSize;     // 32
	constexpr int32 Margin = 2;
	constexpr int32 Cols = 32;

	TArray<FColor> GSprites;       // layer-major Spr x Spr per item
	TArray<FColor> GCells;         // Cell x Cell per atlas entry (items then glyphs)
	TArray<FSlateBrush> GItemBrush, GGlyphBrush;
	TMap<FName, int32> GGlyphIndex;
	UTexture2D* GAtlas = nullptr;
	int32 GAtlasW = 0, GAtlasH = 0;
	bool GBuilt = false;

	// ------------------------------------------------------------------ helpers
	FVector2f AutoUV(int32 Face, const FVector3f& P)
	{
		switch (Face)
		{
		case 0: return FVector2f(P.X, 1.f - P.Y);
		case 1: return FVector2f(P.X, P.Y);
		case 2: return FVector2f(1.f - P.X, 1.f - P.Z);
		case 3: return FVector2f(P.X, 1.f - P.Z);
		case 4: return FVector2f(P.Y, 1.f - P.Z);
		default: return FVector2f(1.f - P.Y, 1.f - P.Z);
		}
	}
	FVector2f RotUV(const FVector2f& UV, uint8 Rot)
	{
		switch (Rot & 3) { case 1: return FVector2f(1.f - UV.Y, UV.X); case 2: return FVector2f(1.f - UV.X, 1.f - UV.Y); case 3: return FVector2f(UV.Y, 1.f - UV.X); default: return UV; }
	}

	FColor TintColor(const FMCBlock& B)
	{
		const FMCBiomeDef& Pl = FMCBiomes::Get((uint8)EMCBiome::Plains);
		switch (B.Tint)
		{
		case EMCTint::Grass: return Pl.Grass;
		case EMCTint::Foliage: return Pl.Foliage;
		case EMCTint::Water: return Pl.Water;
		case EMCTint::Birch: return FColor(128, 167, 85);
		case EMCTint::Spruce: return FColor(97, 153, 97);
		case EMCTint::Custom: return B.TintColor;
		case EMCTint::Stem: return FColor(96, 190, 48);
		case EMCTint::Lily: return FColor(32, 128, 48);
		case EMCTint::Redstone: return FColor(200, 20, 10);
		default: return FColor::White;
		}
	}

	bool TexIsCutout(int32 Layer)
	{
		const TArray<FMCTexDef>& D = FMCTextures::Defs();
		return D.IsValidIndex(Layer) && (D[Layer].Flags & (MCTF_Cutout | MCTF_Translucent)) != 0;
	}

	/** Samples an albedo layer (nearest) returning straight RGBA with tint applied. */
	FLinearColor Sample(int32 Layer, FVector2f UV, bool bTint, const FColor& Tint)
	{
		const FColor* L = MCTexSynth::GetAlbedoLayer(FMath::Max(0, Layer));
		const int32 S = MCTexSynth::LayerSize;
		if (!L) return FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
		const int32 X = FMath::Clamp((int32)(FMath::Frac(UV.X - 1e-4f) * S), 0, S - 1);
		const int32 Y = FMath::Clamp((int32)(FMath::Frac(UV.Y - 1e-4f) * S), 0, S - 1);
		const FColor C = L[Y * S + X];
		const bool bCut = TexIsCutout(Layer);
		FLinearColor Out(C.R / 255.f, C.G / 255.f, C.B / 255.f, bCut ? C.A / 255.f : 1.f);
		const float TintW = bTint ? (bCut ? 1.f : C.A / 255.f) : 0.f;
		if (TintW > 0.f)
		{
			Out.R *= FMath::Lerp(1.f, Tint.R / 255.f, TintW);
			Out.G *= FMath::Lerp(1.f, Tint.G / 255.f, TintW);
			Out.B *= FMath::Lerp(1.f, Tint.B / 255.f, TintW);
		}
		return Out;
	}

	bool UsesFlatIcon(const FMCStateInfo& SI, const FMCBlock& B, const FMCBlockModel& M)
	{
		if (SI.Shape == EMCShape::Cross || SI.Shape == EMCShape::Crop) return true;
		if (SI.Shape != EMCShape::Model) return false;
		if (M.Boxes.Num() == 0) return true;
		switch (B.Model)
		{
		case EMCModel::Torch: case EMCModel::Ladder: case EMCModel::Rail: case EMCModel::Vine: case EMCModel::LilyPad: case EMCModel::Pane:
		case EMCModel::Bars: case EMCModel::Fire: case EMCModel::Lantern: case EMCModel::Chain: case EMCModel::Kelp: case EMCModel::GlowLichen:
		case EMCModel::CaveVines: case EMCModel::AmethystCluster: case EMCModel::Dripstone: case EMCModel::Door: case EMCModel::RedstoneWire:
		case EMCModel::Dripleaf: case EMCModel::BigDripleaf: case EMCModel::Bamboo: case EMCModel::EndRod:
			return true;
		default: return false;
		}
	}

	/** Alpha-weighted 2:1 box downscale. */
	void Half(const FColor* Src, int32 SS, FColor* Dst)
	{
		const int32 DS = SS / 2;
		for (int32 y = 0; y < DS; ++y)
			for (int32 x = 0; x < DS; ++x)
			{
				float R = 0, G = 0, B = 0, A = 0, W = 0;
				for (int32 k = 0; k < 4; ++k)
				{
					const FColor& C = Src[(y * 2 + (k >> 1)) * SS + x * 2 + (k & 1)];
					const float Wt = C.A / 255.f;
					R += C.R * Wt; G += C.G * Wt; B += C.B * Wt; A += C.A; W += Wt;
				}
				Dst[y * DS + x] = W > 0.f ? FColor((uint8)(R / W), (uint8)(G / W), (uint8)(B / W), (uint8)(A / 4.f)) : FColor(0, 0, 0, 0);
			}
	}

	/** Textured isometric render (top, south = left, east = right) of a block state into Size x Size. */
	void RenderIso(FMCState State, int32 Size, FColor* Out)
	{
		const FMCStateInfo& SI = FMCBlocks::Info(State);
		const FMCBlock& B = FMCBlocks::Get(SI.Block);
		FMCBlockModel Model;
		TArray<FMCModelBox> Boxes;
		if (SI.Shape == EMCShape::Cube || SI.Shape == EMCShape::Invisible)
		{
			FMCModelBox Bx(FMCBox(0, 0, 0, 1, 1, 1), 0);
			for (int32 f = 0; f < 6; ++f) { Bx.Tex[f] = SI.Tex[f]; Bx.UVRot[f] = SI.UVRot[f]; }
			if (B.Tint != EMCTint::None) Bx.TintFaces = 0x3F;
			Boxes.Add(Bx);
		}
		else
		{
			FMCBlocks::BuildModel(State, nullptr, FMCBlockPos(0, 0, 0), Model);
			Boxes = Model.Boxes;
		}
		TArray<float> Z; Z.Init(-1e9f, Size * Size);
		for (int32 i = 0; i < Size * Size; ++i) Out[i] = FColor(0, 0, 0, 0);
		// fit the unit cube (with a little margin) into the cell
		const float K = Size * 0.47f;
		const float CX = Size * 0.5f, CY = Size * 0.5f;
		auto Proj = [&](const FVector3f& P) { return FVector2f(CX + (P.X - P.Y) * 0.8660254f * K, CY + ((P.X + P.Y) * 0.5f - P.Z + 0.25f) * K * 0.98f); };
		const FColor Tint = TintColor(B);
		for (const FMCModelBox& Bx : Boxes)
		{
			const FVector3f Mn = Bx.Min, Mx = Bx.Max;
			struct FFace { int32 F; FVector3f O, U, V; float Shade; };
			const FFace Faces[3] = {
				{ 1, FVector3f(Mn.X, Mn.Y, Mx.Z), FVector3f(Mx.X - Mn.X, 0, 0), FVector3f(0, Mx.Y - Mn.Y, 0), 1.f },
				{ 3, FVector3f(Mn.X, Mx.Y, Mx.Z), FVector3f(Mx.X - Mn.X, 0, 0), FVector3f(0, 0, -(Mx.Z - Mn.Z)), 0.8f },
				{ 5, FVector3f(Mx.X, Mx.Y, Mx.Z), FVector3f(0, -(Mx.Y - Mn.Y), 0), FVector3f(0, 0, -(Mx.Z - Mn.Z)), 0.64f },
			};
			for (const FFace& Fc : Faces)
			{
				const int16 Tex = Bx.Tex[Fc.F];
				if (Tex < 0) continue;
				const FVector2f O2 = Proj(Fc.O), U2 = Proj(Fc.O + Fc.U) - O2, V2 = Proj(Fc.O + Fc.V) - O2;
				const float Det = U2.X * V2.Y - U2.Y * V2.X;
				if (FMath::Abs(Det) < 1e-4f) continue;
				const FVector2f P1 = O2 + U2, P2 = O2 + V2, P3 = O2 + U2 + V2;
				const int32 X0 = FMath::Max(0, FMath::FloorToInt(FMath::Min(FMath::Min(O2.X, P1.X), FMath::Min(P2.X, P3.X))));
				const int32 X1 = FMath::Min(Size - 1, FMath::CeilToInt(FMath::Max(FMath::Max(O2.X, P1.X), FMath::Max(P2.X, P3.X))));
				const int32 Y0 = FMath::Max(0, FMath::FloorToInt(FMath::Min(FMath::Min(O2.Y, P1.Y), FMath::Min(P2.Y, P3.Y))));
				const int32 Y1 = FMath::Min(Size - 1, FMath::CeilToInt(FMath::Max(FMath::Max(O2.Y, P1.Y), FMath::Max(P2.Y, P3.Y))));
				const bool bTint = (Bx.TintFaces & (1 << Fc.F)) != 0 || B.Tint != EMCTint::None;
				for (int32 y = Y0; y <= Y1; ++y)
					for (int32 x = X0; x <= X1; ++x)
					{
						const FVector2f D(x + 0.5f - O2.X, y + 0.5f - O2.Y);
						const float S = (D.X * V2.Y - D.Y * V2.X) / Det;
						const float T = (U2.X * D.Y - U2.Y * D.X) / Det;
						if (S < 0.f || S > 1.f || T < 0.f || T > 1.f) continue;
						const FVector3f P = Fc.O + Fc.U * S + Fc.V * T;
						const float Depth = P.X + P.Y + P.Z;
						const int32 I = y * Size + x;
						if (Depth < Z[I]) continue;
						FVector2f UV;
						if (Bx.bAutoUV) UV = RotUV(AutoUV(Fc.F, P), Bx.UVRot[Fc.F]);
						else { const FVector2f A = AutoUV(Fc.F, P); const FVector4f& R = Bx.UV[Fc.F]; UV = FVector2f(FMath::Lerp(R.X, R.Z, A.X), FMath::Lerp(R.Y, R.W, A.Y)); }
						FLinearColor C = Sample(Tex, UV, bTint, Tint);
						if (C.A < 0.5f) continue;
						C.R *= Fc.Shade * (Bx.Color.R / 255.f); C.G *= Fc.Shade * (Bx.Color.G / 255.f); C.B *= Fc.Shade * (Bx.Color.B / 255.f);
						Z[I] = Depth;
						Out[I] = FColor((uint8)FMath::Clamp(C.R * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(C.G * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(C.B * 255.f, 0.f, 255.f), 255);
					}
			}
		}
	}

	/** Flat icon from the block's texture (plants, torches, rails...). Doors stack their two halves. */
	void RenderFlat(FMCState State, int32 Size, FColor* Out)
	{
		const FMCStateInfo& SI = FMCBlocks::Info(State);
		const FMCBlock& B = FMCBlocks::Get(SI.Block);
		const FColor Tint = TintColor(B);
		const bool bTint = B.Tint != EMCTint::None;
		int16 Tex = SI.Tex[2] >= 0 ? SI.Tex[2] : SI.Tex[0];
		if (SI.Shape == EMCShape::Cross || SI.Shape == EMCShape::Crop) Tex = SI.Tex[0];
		const bool bDoor = B.Model == EMCModel::Door;
		const int16 TopTex = bDoor && B.TexAlt[0] >= 0 ? B.TexAlt[0] : Tex;
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				FVector2f UV((x + 0.5f) / Size, (y + 0.5f) / Size);
				int16 L = Tex;
				if (bDoor)
				{
					if (UV.X < 0.25f || UV.X > 0.75f) { Out[y * Size + x] = FColor(0, 0, 0, 0); continue; }
					UV.X = (UV.X - 0.25f) * 2.f;
					L = UV.Y < 0.5f ? TopTex : Tex;
					UV.Y = FMath::Frac(UV.Y * 2.f);
				}
				const FLinearColor C = Sample(L, UV, bTint, Tint);
				Out[y * Size + x] = C.A < 0.5f ? FColor(0, 0, 0, 0) : FColor((uint8)FMath::Clamp(C.R * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(C.G * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(C.B * 255.f, 0.f, 255.f), 255);
			}
	}

	void Upscale2x(const FColor* Src, int32 SS, FColor* Dst)
	{
		for (int32 y = 0; y < SS * 2; ++y) for (int32 x = 0; x < SS * 2; ++x) Dst[y * SS * 2 + x] = Src[(y / 2) * SS + x / 2];
	}
}

namespace MCIcons
{
	bool HasGlint(const FMCItemStack& S) { return S.IsEnchanted() || S.Item().bGlint; }

	void Build()
	{
		if (GBuilt) return;
		GBuilt = true;
		const double T0 = FPlatformTime::Seconds();
		FMCBlocks::Init();
		FMCItems::Init();
		if (!MCTexSynth::GetAlbedoLayer(0))
		{
			TArray<FColor> A, N, O;
			MCTexSynth::BuildTerrain(A, N, O); // keeps an albedo copy for icon sampling (disk cached)
		}
		const int32 NumItems = FMCItems::Num();
		const TArray<FName>& Glyphs = GlyphNames();
		const int32 NumCells = NumItems + Glyphs.Num();
		GSprites.SetNumZeroed((int64)Spr * Spr * NumItems);
		GCells.SetNumZeroed((int64)Cell * Cell * NumCells);

		ParallelFor(NumItems, [&](int32 i)
		{
			const FMCItem& It = FMCItems::Get((FMCItemId)i);
			FColor* CellPx = GCells.GetData() + (int64)Cell * Cell * i;
			FColor* SprPx = GSprites.GetData() + (int64)Spr * Spr * i;
			if (i == 0) return;
			if (It.Block != 0 && It.Kind == EMCItemKind::Block)
			{
				const FMCBlock& B = FMCBlocks::Get(It.Block);
				const FMCStateInfo& SI = FMCBlocks::Info(B.BaseState);
				FMCBlockModel M;
				if (SI.Shape == EMCShape::Model) FMCBlocks::BuildModel(B.BaseState, nullptr, FMCBlockPos(0, 0, 0), M);
				if (UsesFlatIcon(SI, B, M))
				{
					RenderFlat(B.BaseState, Cell, CellPx);
				}
				else
				{
					TArray<FColor> Big; Big.SetNumZeroed(Cell * Cell * 4);
					RenderIso(B.BaseState, Cell * 2, Big.GetData());
					Half(Big.GetData(), Cell * 2, CellPx);
				}
				Half(CellPx, Cell, SprPx);
				return;
			}
			if (!PaintItem(It, SprPx))
			{
				for (int32 k = 0; k < Spr * Spr; ++k) SprPx[k] = FColor(0, 0, 0, 0);
			}
			Upscale2x(SprPx, Spr, CellPx);
		});
		TArray<FColor> G; G.SetNumZeroed(Spr * Spr);
		for (int32 g = 0; g < Glyphs.Num(); ++g)
		{
			PaintGlyph(Glyphs[g], G.GetData());
			Upscale2x(G.GetData(), Spr, GCells.GetData() + (int64)Cell * Cell * (NumItems + g));
			GGlyphIndex.Add(Glyphs[g], g);
		}
		GAtlasW = Cols * (Cell + Margin) + Margin;
		GAtlasH = FMath::DivideAndRoundUp(NumCells, Cols) * (Cell + Margin) + Margin;
		UE_LOG(LogOpus55, Log, TEXT("Item icons built: %d items, %d glyphs in %.2fs"), NumItems, Glyphs.Num(), FPlatformTime::Seconds() - T0);
	}

	int32 SpriteLayer(FMCItemId Id) { return (int32)FMath::Clamp<int32>(Id, 0, FMath::Max(0, FMCItems::Num() - 1)); }

	FColor ItemTint(const FMCBlock& B)
	{
		return TintColor(B);
	}

	const FColor* SpritePixels(int32 Layer)
	{
		const int64 Offset = (int64)Spr * Spr * Layer;
		if (Layer < 0 || Offset + Spr * Spr > GSprites.Num()) return nullptr;
		return GSprites.GetData() + Offset;
	}

	UTexture2DArray* CreateSpriteArray(UObject* Outer)
	{
		Build();
		const int32 N = FMCItems::Num();
		return N > 0 ? MCTexSynth::CreateArray(Outer, Spr, N, GSprites, true, false, true) : nullptr;
	}

	UTexture2D* GetAtlas()
	{
		Build();
		if (GAtlas) return GAtlas;
		const int32 NumItems = FMCItems::Num(), NumCells = NumItems + GlyphNames().Num();
		TArray<FColor> Px; Px.Init(FColor(0, 0, 0, 0), GAtlasW * GAtlasH);
		auto CellOrigin = [&](int32 Index) { const int32 C = Index % Cols, R = Index / Cols; return FIntPoint(Margin + C * (Cell + Margin), Margin + R * (Cell + Margin)); };
		for (int32 i = 0; i < NumCells; ++i)
		{
			const FIntPoint O = CellOrigin(i);
			const FColor* Src = GCells.GetData() + (int64)Cell * Cell * i;
			for (int32 y = 0; y < Cell; ++y) FMemory::Memcpy(&Px[(O.Y + y) * GAtlasW + O.X], Src + y * Cell, Cell * sizeof(FColor));
		}
		UTexture2D* Tex = UTexture2D::CreateTransient(GAtlasW, GAtlasH, PF_B8G8R8A8, TEXT("MCIconAtlas"));
		if (!Tex) return nullptr;
		Tex->SRGB = true;
		Tex->Filter = TF_Bilinear;
		Tex->LODGroup = TEXTUREGROUP_UI;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		Tex->NeverStream = true;
		void* Data = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Px.GetData(), (int64)GAtlasW * GAtlasH * sizeof(FColor));
		Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
		Tex->UpdateResource();
		Tex->AddToRoot();
		GAtlas = Tex;
		auto MakeBrush = [&](int32 Index)
		{
			const FIntPoint O = CellOrigin(Index);
			FSlateBrush B;
			B.SetResourceObject(Tex);
			B.SetImageSize(FVector2D(Cell, Cell));
			B.DrawAs = ESlateBrushDrawType::Image;
			B.Tiling = ESlateBrushTileType::NoTile;
			B.SetUVRegion(FBox2f(FVector2f((O.X + 0.5f) / GAtlasW, (O.Y + 0.5f) / GAtlasH), FVector2f((O.X + Cell - 0.5f) / GAtlasW, (O.Y + Cell - 0.5f) / GAtlasH)));
			return B;
		};
		GItemBrush.Reset(); GGlyphBrush.Reset();
		for (int32 i = 0; i < NumItems; ++i) GItemBrush.Add(MakeBrush(i));
		for (int32 g = 0; g < GlyphNames().Num(); ++g) GGlyphBrush.Add(MakeBrush(NumItems + g));
		return Tex;
	}

	const FSlateBrush* GetBrush(FMCItemId Id)
	{
		if (!GAtlas) GetAtlas();
		return GItemBrush.IsValidIndex(Id) ? &GItemBrush[Id] : (GItemBrush.Num() ? &GItemBrush[0] : nullptr);
	}

	const FSlateBrush* GetGlyph(FName Glyph)
	{
		if (!GAtlas) GetAtlas();
		const int32* I = GGlyphIndex.Find(Glyph);
		return (I && GGlyphBrush.IsValidIndex(*I)) ? &GGlyphBrush[*I] : nullptr;
	}
}
