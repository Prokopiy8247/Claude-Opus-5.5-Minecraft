// Mob / player rigs: hierarchies of rigid parts (Blender-authored static meshes, with box fallbacks)
// animated procedurally with Minecraft-style formulas. Also the visual component for item stacks.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/MCCore.h"
#include "Render/MCMesher.h"
#include "MCRig.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMCChunkMeshComponent;
struct FMCItemStack;
class UMCChunkMeshComponent;
class UMaterialInstanceDynamic;

enum class EMCRigRole : uint8
{
	None, Body, Head, Jaw, Neck, LegFL, LegFR, LegBL, LegBR, LegML, LegMR, ArmL, ArmR, Tail, Tail2, WingL, WingR,
	EarL, EarR, Tentacle, FinL, FinR, Snout, Horn, Misc, Spin, Rod, Mouth, Shell
};

/** One rigid part. Coordinates in blocks, entity space (origin = feet centre, +X forward, +Y right, +Z up). */
struct UNREAL_MINECRAFT_API FMCRigPart
{
	FName Name;
	int32 Parent = -1;
	EMCRigRole Role = EMCRigRole::None;
	int32 Index = 0;                        // e.g. tentacle number, leg pair number
	FVector3f Pivot = FVector3f::ZeroVector; // rest pivot (entity space)
	FVector3f BoxMin = FVector3f::ZeroVector, BoxMax = FVector3f::ZeroVector; // fallback box relative to the pivot
	FColor Color = FColor::White;           // fallback colour
	float Phase = 0.f;                      // animation phase offset
	bool bEmissive = false;
};

struct UNREAL_MINECRAFT_API FMCRigDef
{
	FName Id;
	TArray<FMCRigPart> Parts;
	float Scale = 1.f;
	FVector3f HandOffset = FVector3f(0, 0, 0); // held item offset in the right arm part space
	int32 RightArm = -1, LeftArm = -1, HeadPart = -1;
	int32 FindPart(FName Name) const { for (int32 i = 0; i < Parts.Num(); ++i) if (Parts[i].Name == Name) return i; return -1; }
};

namespace MCRigs
{
	UNREAL_MINECRAFT_API void Init();
	UNREAL_MINECRAFT_API const FMCRigDef* Find(FName Id);
	UNREAL_MINECRAFT_API const TArray<FMCRigDef>& All();
	/** Asset path of an authored part mesh: /Game/Opus55Minecraft/Mobs/<rig>/SM_<rig>__<part>. */
	UNREAL_MINECRAFT_API FString PartMeshPath(FName Rig, FName Part);
}

/** Procedural animation inputs updated by the owning entity every frame. */
struct FMCRigPose
{
	float LimbSwing = 0.f;          // walk phase (Minecraft: distance walked * 0.6662 ...)
	float LimbAmount = 0.f;         // 0..1
	float Age = 0.f;                // ticks (fractional)
	float HeadYaw = 0.f;            // degrees relative to the body
	float HeadPitch = 0.f;          // degrees (+ = looking up)
	float Attack = 0.f;             // 0..1 swing progress
	float Death = 0.f;              // 0..1 (tip over)
	float Hurt = 0.f;               // 0..1 red flash
	float Special = 0.f;            // rig specific (creeper swell, wing flap, mouth open, charge)
	float Special2 = 0.f;
	bool bSitting = false, bBaby = false, bSneaking = false, bSwimming = false, bFlying = false, bRiding = false;
	bool bAggressive = false, bHoldingItem = false, bBlocking = false, bSleeping = false;
};

UCLASS(ClassGroup = (Opus55), meta = (BlueprintSpawnableComponent))
class UNREAL_MINECRAFT_API UMCRigComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UMCRigComponent(const FObjectInitializer& ObjectInitializer);

	void SetRig(FName RigId, const FColor& InTint = FColor::White);
	void SetTint(const FColor& C);
	void SetHurtFlash(float Amount);
	void SetGlow(float Amount);
	/** Local light (entity material "Fill" / "TorchLight"); parameters are only pushed when they change. */
	void SetLighting(float Fill, float Torch);
	void SetHidden(bool bHide);
	void SetPartVisible(FName Part, bool bShow);
	/** Attach a component (held item) to a part. */
	void AttachToPart(USceneComponent* Child, FName Part, const FVector& OffsetBlocks = FVector::ZeroVector, const FRotator& Rot = FRotator::ZeroRotator);
	USceneComponent* GetPart(FName Part) const;
	USceneComponent* GetPartByRole(EMCRigRole Role, int32 Index = 0) const;

	/** Evaluate and apply the pose. */
	void ApplyPose(const FMCRigPose& Pose);
	/** True when any part was drawn within the last Tolerance seconds (animation budget for off-screen mobs). */
	bool IsOnScreen(float Tolerance = 0.25f) const;
	/** Direct part control (bosses): relative rotation in degrees. */
	void SetPartRotation(int32 PartIndex, const FRotator& R);
	void SetPartOffset(int32 PartIndex, const FVector& OffsetBlocks);

	const FMCRigDef* Rig = nullptr;
	FName RigId;
	float ModelScale = 1.f;

	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Pivots;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;

private:
	FColor Tint = FColor::White;
	/** Tint the rig was set up with (the mob definition's). Authored meshes already carry their
	 *  colours in the vertex data, so they receive Tint relative to this (variants, dyes, charge). */
	FColor BaseTint = FColor::White;
	float LightFill = -1.f, LightTorch = -1.f;
	TArray<bool> PartAuthored;
	TArray<FRotator> Extra;
	TArray<FVector> ExtraOffset;
	void Build();
	void Clear();
	FLinearColor PartTint(int32 PartIndex) const;
};

/** Visual for one item stack / block state (dropped items, item frames, held items, falling blocks). */
UCLASS(ClassGroup = (Opus55), meta = (BlueprintSpawnableComponent))
class UNREAL_MINECRAFT_API UMCItemVisualComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UMCItemVisualComponent(const FObjectInitializer& ObjectInitializer);
	virtual void OnRegister() override;

	/** Mode: 0 world (dropped item), 1 first-person hand, 2 third-person hand, 3 item frame, 4 head slot. */
	void SetStack(const FMCItemStack& Stack, int32 Mode = 0);
	/** Full-size block (falling blocks, TNT, minecart contents). */
	void SetBlockState(uint16 State, float SizeBlocks = 1.f);
	void Clear();
	void SetHiddenAll(bool bHide);
	void SetFlash(float Amount); // TNT white flash
	bool IsBlockModel() const { return bIsBlock; }
	/** Item currently shown (for pose tweaks such as the shield). */
	FName ItemName;
	bool IsEmptyVisual() const { return CurrentId == 0 && CurrentState == 0; }

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Authored;
	UPROPERTY(Transient) TObjectPtr<UMCChunkMeshComponent> Generated;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlashMID;

	/** Extruded 16x16 icon geometry for an item (Minecraft-style item model). */
	static TUniquePtr<struct FMCChunkMeshData> BuildIconMesh(int32 IconLayer, int32 Resolution, float Thickness);
	/** Single block geometry centred on the origin. */
	static TUniquePtr<struct FMCChunkMeshData> BuildBlockMesh(uint16 State, float Size);

private:
	uint16 CurrentId = 0;
	uint16 CurrentState = 0;
	int32 CurrentMode = -1;
	bool bIsBlock = false;
};
