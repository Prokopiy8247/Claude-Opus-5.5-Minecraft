// Item icons: procedural pixel-art sprites for non-block items, isometric renders of block items, a UI atlas with
// Slate brushes and a texture array of item sprites used by in-world item models (dropped / held items).
#pragma once

#include "CoreMinimal.h"
#include "Items/MCItems.h"

class UTexture2D;
class UTexture2DArray;
struct FSlateBrush;

namespace MCIcons
{
	/** Sprite resolution (item sprites; also used for extruded in-world items). */
	constexpr int32 SpriteSize = 32;
	/** UI atlas cell size. */
	constexpr int32 CellSize = 64;

	/** Builds every icon (idempotent). Requires blocks, items and terrain textures. */
	UNREAL_MINECRAFT_API void Build();
	/** Sprite layer for an item (index into the sprite array; 0 = missing). */
	UNREAL_MINECRAFT_API int32 SpriteLayer(FMCItemId Id);
	/** CPU copy of a sprite layer (SpriteSize^2 texels, row 0 = top), or nullptr before Build(). */
	UNREAL_MINECRAFT_API const FColor* SpritePixels(int32 Layer);
	/** Texture array of item sprites (one layer per item id). */
	UNREAL_MINECRAFT_API UTexture2DArray* CreateSpriteArray(UObject* Outer);
	/** UI atlas texture (rooted, created on first use). */
	UNREAL_MINECRAFT_API UTexture2D* GetAtlas();
	/** Slate brush showing one item icon. */
	UNREAL_MINECRAFT_API const FSlateBrush* GetBrush(FMCItemId Id);
	/** Brush of a named UI glyph drawn into the atlas (heart, hunger, armor, xp...). */
	UNREAL_MINECRAFT_API const FSlateBrush* GetGlyph(FName Glyph);
	/** Tint of a block outside the world (inventory icons, held and dropped blocks): plains colours, dye colours. */
	UNREAL_MINECRAFT_API FColor ItemTint(const struct FMCBlock& B);
	/** True when the item should be drawn with the enchantment glint. */
	UNREAL_MINECRAFT_API bool HasGlint(const FMCItemStack& S);

	/** Procedural sprite painter (MCIconPainter.cpp). Returns false if the item has no dedicated sprite. */
	bool PaintItem(const FMCItem& Item, FColor* Out /* SpriteSize^2 */);
	/** UI glyphs (hearts, food, armor, bubbles, crosshair...) rendered at 16x16 design units. */
	void PaintGlyph(FName Glyph, FColor* Out /* SpriteSize^2 */);
	const TArray<FName>& GlyphNames();
}
