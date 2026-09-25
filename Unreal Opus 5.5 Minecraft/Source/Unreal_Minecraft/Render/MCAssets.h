// Cached access to engine / project assets used by runtime-built visuals (always with safe fallbacks).
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USceneComponent;

namespace MCAssets
{
	/** Loaded asset or nullptr (cached, rooted). */
	UNREAL_MINECRAFT_API UStaticMesh* Mesh(const TCHAR* Path);
	UNREAL_MINECRAFT_API UMaterialInterface* Material(const TCHAR* Path);

	UNREAL_MINECRAFT_API UStaticMesh* Cube();      // 100 UU cube centred on the origin
	UNREAL_MINECRAFT_API UStaticMesh* Sphere();    // 100 UU diameter
	UNREAL_MINECRAFT_API UStaticMesh* Cylinder();  // 100 UU tall, 100 UU diameter
	UNREAL_MINECRAFT_API UStaticMesh* Plane();     // 100x100 UU in XY

	/** Vertex-colour lit material with Tint / Hurt / Glow parameters (M_MCEntity). */
	UNREAL_MINECRAFT_API UMaterialInterface* EntityMaterial();
	/** Emissive material with a Color parameter (M_MCGlow), used for beams, orbs, lightning. */
	UNREAL_MINECRAFT_API UMaterialInterface* GlowMaterial();
	/** Translucent additive material (M_MCBeam) for beacon / crystal beams. */
	UNREAL_MINECRAFT_API UMaterialInterface* BeamMaterial();

	UNREAL_MINECRAFT_API UMaterialInstanceDynamic* MakeMID(UObject* Outer, UMaterialInterface* Parent, const FLinearColor& Color, float Emissive = 0.f);

	/** Creates, attaches and registers a simple mesh component on an actor at runtime. */
	UNREAL_MINECRAFT_API UStaticMeshComponent* AddMeshPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& LocUU, const FVector& Scale,
		UMaterialInterface* Mat, const FLinearColor& Color, float Emissive = 0.f, bool bShadow = true);
}
