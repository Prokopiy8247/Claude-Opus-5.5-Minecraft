// Minecraft-style sky/block light propagation.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"

class FMCChunk;

namespace MCLighting
{
	/**
	 * Compute exact light for the centre chunk of a 3x3 neighbourhood (index = (dx+1) + (dy+1)*3).
	 * OutLight receives NumSections*4096 packed bytes (sky << 4 | block) in section order.
	 * Thread safe (reads only).
	 */
	UNREAL_MINECRAFT_API void ComputeRegion(const FMCChunk* const Near[9], EMCDimension Dim, TArray<uint8>& OutLight);
}
