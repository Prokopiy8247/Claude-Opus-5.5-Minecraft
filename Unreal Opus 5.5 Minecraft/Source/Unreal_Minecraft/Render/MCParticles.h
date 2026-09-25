// CPU particle system (block breaking, smoke, flames, drips, portals, explosions, crits, hearts, rain splashes...).
// Particles are simulated at frame rate with simple collision and rendered as camera-facing quads by one
// procedural mesh per frame (textures come from the terrain array or the particle sprite layers).
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/MCCore.h"
#include "MCParticles.generated.h"

class FMCWorld;
class UMCChunkMeshComponent;
class UMaterialInterface;

struct FMCParticle
{
	FVector Pos = FVector::ZeroVector;   // block units
	FVector Vel = FVector::ZeroVector;   // blocks / second
	float Age = 0.f, Life = 1.f;
	float Size = 0.1f;                   // blocks
	float Gravity = 0.f;                 // blocks / s^2
	float Drag = 0.98f;
	FLinearColor Color = FLinearColor::White;
	int16 TexLayer = 0;                  // terrain layer (block particles) or sprite layer
	FVector2f UVMin = FVector2f(0, 0), UVMax = FVector2f(1, 1);
	uint8 Kind = 0;                      // 0 sprite, 1 block chip
	bool bCollide = true;
	bool bEmissive = false;
	bool bShrink = false;
	bool bFade = true;
	bool bOnGround = false;
	float Spin = 0.f;
};

UCLASS()
class UNREAL_MINECRAFT_API UMCParticles : public USceneComponent
{
	GENERATED_BODY()
public:
	UMCParticles(const FObjectInitializer& ObjectInitializer);

	void SetWorld(FMCWorld* InWorld) { World = InWorld; Particles.Reset(); }
	/** Named effect (see MCParticles.cpp for the list). */
	void Spawn(FName Type, const FVector& PosBlocks, int32 Count, float Spread, const FVector& Vel, FColor Color);
	/** Block crumbs using the block's textures. */
	void SpawnBlockBreak(const FMCBlockPos& P, uint16 State);
	void SpawnBlockHit(const FMCBlockPos& P, uint16 State, EMCFace Face);
	void Add(const FMCParticle& P);
	void UpdateParticles(float DeltaSeconds, const FVector& CameraPosUU, const FRotator& CameraRot);
	void Clear() { Particles.Reset(); }
	int32 Num() const { return Particles.Num(); }

	int32 MaxParticles = 6000;

private:
	FMCWorld* World = nullptr;
	TArray<FMCParticle> Particles;
	UPROPERTY(Transient) TObjectPtr<UMCChunkMeshComponent> Mesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> Materials;
	FMCRandom Rand;
	void EnsureMesh();
};
