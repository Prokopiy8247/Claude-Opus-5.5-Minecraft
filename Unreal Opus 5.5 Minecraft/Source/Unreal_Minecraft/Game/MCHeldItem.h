// First-person hand: the arm and the held item with Minecraft-style equip, swing, bob, eat and bow-draw animation.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Items/MCItems.h"
#include "MCHeldItem.generated.h"

class UMCItemVisualComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AMCPlayer;

UCLASS(ClassGroup = (Opus55))
class UNREAL_MINECRAFT_API UMCHeldItemComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UMCHeldItemComponent(const FObjectInitializer& ObjectInitializer);

	/** Rebuild the visual if the stack changed. */
	void SetStack(const FMCItemStack& Stack);
	/** Per-frame pose (called by the player after the camera moved). */
	void UpdatePose(const AMCPlayer* P, float Alpha, float DeltaSeconds);
	void SetHandVisible(bool bVisible);

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Hand;          // animated pivot
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Arm;      // player arm (authored or box)
	UPROPERTY(Transient) TObjectPtr<UMCItemVisualComponent> Item;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ArmMID;

	float EquipProgress = 1.f, PrevEquipProgress = 1.f;

private:
	FMCItemId ShownId = 0;
	int32 ShownDamage = -1;
	bool bShownEmpty = true;
	bool bAuthoredArm = false;     // SM_PlayerArm from the Blender pipeline (vertex-coloured, untinted)
	float SwayYaw = 0.f, SwayPitch = 0.f;
	float LastYaw = 0.f, LastPitch = 0.f;
	void EnsureParts();
};
