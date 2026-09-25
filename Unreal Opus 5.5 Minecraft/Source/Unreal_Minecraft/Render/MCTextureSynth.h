// Runtime procedural texture synthesizer: turns every FMCTexDef recipe into albedo / normal / ORME texels
// (ORME = ambient occlusion, roughness, metallic, emissive). Results are cached on disk between runs.
#pragma once

#include "CoreMinimal.h"

class UTexture2DArray;
struct FMCTexDef;

namespace MCTexSynth
{
	/** Resolution of one terrain layer (square). */
	constexpr int32 LayerSize = 64;
	constexpr int32 NumCrackStages = 10;

	/** Soft particle sprites appended after the crack stages. */
	enum EParticleSprite : int32
	{
		PS_Smoke, PS_Flame, PS_Heart, PS_Crit, PS_Spark, PS_Bubble, PS_Portal, PS_Note, PS_Drip, PS_Snow, PS_Rain, PS_Glint,
		PS_Angry, PS_Happy, PS_SoulFlame, PS_Explosion, PS_Poof, PS_Rune, PS_Dust, PS_Sweep, PS_Leaf, PS_Count
	};

	/** Number of layers in the terrain arrays (texture definitions + destroy stages). */
	UNREAL_MINECRAFT_API int32 NumLayers();
	UNREAL_MINECRAFT_API int32 CrackLayer(int32 Stage);
	UNREAL_MINECRAFT_API int32 ParticleLayer(int32 Sprite);

	/** Generates one texture definition (thread safe). Buffers hold Size*Size texels. */
	UNREAL_MINECRAFT_API void GenerateDef(const FMCTexDef& Def, int32 Size, FColor* Albedo, FColor* Normal, FColor* Orme);

	/** Builds (or loads from cache) all terrain layers, layer-major. */
	UNREAL_MINECRAFT_API void BuildTerrain(TArray<FColor>& Albedo, TArray<FColor>& Normal, TArray<FColor>& Orme);

	/** Albedo texels of one terrain layer (valid after BuildTerrain; used by the icon renderer). */
	UNREAL_MINECRAFT_API const FColor* GetAlbedoLayer(int32 Layer);

	/** Creates a transient texture array with a full mip chain from layer-major texels. */
	UNREAL_MINECRAFT_API UTexture2DArray* CreateArray(UObject* Outer, int32 Size, int32 Layers, const TArray<FColor>& Texels, bool bSRGB, bool bNormalMap, bool bCutoutAware);
}
